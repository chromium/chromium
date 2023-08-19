// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace extensions {

namespace {
const char kIdentityGaiaIdPref[] = "identity_gaia_id";
}

IdentityAPI::IdentityAPI(content::BrowserContext* context)
    : IdentityAPI(Profile::FromBrowserContext(context),
                  IdentityManagerFactory::GetForProfile(
                      Profile::FromBrowserContext(context)),
                  ExtensionPrefs::Get(context),
                  EventRouter::Get(context)) {}

IdentityAPI::~IdentityAPI() {}

IdentityMintRequestQueue* IdentityAPI::mint_queue() { return &mint_queue_; }

IdentityTokenCache* IdentityAPI::token_cache() {
  return &token_cache_;
}

void IdentityAPI::SetGaiaIdForExtension(const std::string& extension_id,
                                        const std::string& gaia_id) {
  DCHECK(!gaia_id.empty());
  extension_prefs_->UpdateExtensionPref(extension_id, kIdentityGaiaIdPref,
                                        base::Value(gaia_id));
}

absl::optional<std::string> IdentityAPI::GetGaiaIdForExtension(
    const std::string& extension_id) {
  std::string gaia_id;
  if (!extension_prefs_->ReadPrefAsString(extension_id, kIdentityGaiaIdPref,
                                          &gaia_id)) {
    return absl::nullopt;
  }
  return gaia_id;
}

void IdentityAPI::EraseGaiaIdForExtension(const std::string& extension_id) {
  extension_prefs_->UpdateExtensionPref(extension_id, kIdentityGaiaIdPref,
                                        absl::nullopt);
}

void IdentityAPI::EraseStaleGaiaIdsForAllExtensions() {
  // Refresh tokens haven't been loaded yet. Wait for OnRefreshTokensLoaded() to
  // fire.
  if (!identity_manager_->AreRefreshTokensLoaded())
    return;
  std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();
  for (const ExtensionId& extension_id : extension_prefs_->GetExtensions()) {
    absl::optional<std::string> gaia_id = GetGaiaIdForExtension(extension_id);
    if (!gaia_id)
      continue;
    if (!base::Contains(accounts, *gaia_id, &CoreAccountInfo::gaia)) {
      EraseGaiaIdForExtension(extension_id);
    }
  }
}

void IdentityAPI::Shutdown() {
  on_shutdown_callback_list_.Notify();
  identity_manager_->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<IdentityAPI>>::
    DestructorAtExit g_identity_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<IdentityAPI>* IdentityAPI::GetFactoryInstance() {
  return g_identity_api_factory.Pointer();
}

base::CallbackListSubscription IdentityAPI::RegisterOnShutdownCallback(
    base::OnceClosure cb) {
  return on_shutdown_callback_list_.Add(std::move(cb));
}

bool IdentityAPI::AreExtensionsRestrictedToPrimaryAccount() {
  return !AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
         !AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile_);
}

IdentityAPI::IdentityAPI(Profile* profile,
                         signin::IdentityManager* identity_manager,
                         ExtensionPrefs* extension_prefs,
                         EventRouter* event_router)
    : profile_(profile),
      identity_manager_(identity_manager),
      extension_prefs_(extension_prefs),
      event_router_(event_router) {
  identity_manager_->AddObserver(this);
  EraseStaleGaiaIdsForAllExtensions();
}

void IdentityAPI::OnRefreshTokensLoaded() {
  EraseStaleGaiaIdsForAllExtensions();
}

void IdentityAPI::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Refresh tokens are sometimes made available in contexts where
  // AccountTrackerService is not tracking the account in question. Bail out in
  // these cases.
  if (account_info.gaia.empty())
    return;

  FireOnAccountSignInChanged(account_info.gaia, true);
}

void IdentityAPI::OnExtendedAccountInfoRemoved(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  EraseStaleGaiaIdsForAllExtensions();
  FireOnAccountSignInChanged(account_info.gaia, false);
}

void IdentityAPI::FireOnAccountSignInChanged(const std::string& gaia_id,
                                             bool is_signed_in) {
  DCHECK(!gaia_id.empty());
  api::identity::AccountInfo api_account_info;
  api_account_info.id = gaia_id;

  auto args =
      api::identity::OnSignInChanged::Create(api_account_info, is_signed_in);
  std::unique_ptr<Event> event(new Event(
      events::IDENTITY_ON_SIGN_IN_CHANGED,
      api::identity::OnSignInChanged::kEventName, std::move(args), profile_));

  if (on_signin_changed_callback_for_testing_)
    on_signin_changed_callback_for_testing_.Run(event.get());

  event_router_->BroadcastEvent(std::move(event));
}

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());

  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

}  // namespace extensions
