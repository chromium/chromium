// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#endif

using signin::ConsentLevel;
using signin::PrimaryAccountChangeEvent;

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

std::optional<std::string> IdentityAPI::GetGaiaIdForExtension(
    const std::string& extension_id) {
  std::string gaia_id;
  if (!extension_prefs_->ReadPrefAsString(extension_id, kIdentityGaiaIdPref,
                                          &gaia_id)) {
    return std::nullopt;
  }
  return gaia_id;
}

void IdentityAPI::EraseGaiaIdForExtension(const std::string& extension_id) {
  extension_prefs_->UpdateExtensionPref(extension_id, kIdentityGaiaIdPref,
                                        std::nullopt);
}

void IdentityAPI::EraseStaleGaiaIdsForAllExtensions() {
  // Refresh tokens haven't been loaded yet. Wait for OnRefreshTokensLoaded() to
  // fire.
  if (!identity_manager_->AreRefreshTokensLoaded())
    return;
  auto accounts = GetAccountsWithRefreshTokensForExtensions();
  for (const ExtensionId& extension_id : extension_prefs_->GetExtensions()) {
    std::optional<std::string> gaia_id = GetGaiaIdForExtension(extension_id);
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

bool IdentityAPI::HasAccessToChromeAccounts() const {
  return identity_manager_->HasPrimaryAccount(ConsentLevel::kSignin);
}

std::vector<CoreAccountInfo>
IdentityAPI::GetAccountsWithRefreshTokensForExtensions() {
  if (!HasAccessToChromeAccounts()) {
    return {};
  }
  return identity_manager_->GetAccountsWithRefreshTokens();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void IdentityAPI::MaybeShowChromeSigninDialog(std::string_view extension_name,
                                              base::OnceClosure on_complete) {
  if (HasAccessToChromeAccounts() ||
      identity_manager_->GetAccountsWithRefreshTokens().empty()) {
    DVLOG(1) << "The user is not signed in on the web!";
    std::move(on_complete).Run();
    return;
  }

  if (is_chrome_signin_dialog_open_) {
    DVLOG(1) << "Chrome sign in dialog is already open, extensions are not "
                "allowed to trigger more than one";
    on_chrome_signin_dialog_completed_.push_back(std::move(on_complete));
    return;
  }

  // Used in unittests to avoid creating a browser.
  if (skip_ui_for_testing_callback_) {
    HandleSkipUIForTesting(std::move(on_complete));  // IN-TEST
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
  Browser* browser = displayer.browser();
  if (!browser) {
    DVLOG(1) << "Could not create a browser to show Extensions Chrome Sign in "
                "dialog.";
    std::move(on_complete).Run();
    return;
  }
  on_chrome_signin_dialog_completed_.push_back(std::move(on_complete));
  is_chrome_signin_dialog_open_ = true;
  browser->signin_view_controller()->MaybeShowChromeSigninDialogForExtensions(
      extension_name,
      base::BindOnce(&IdentityAPI::OnChromeSigninDialogDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdentityAPI::OnChromeSigninDialogDestroyed() {
  is_chrome_signin_dialog_open_ = false;
  std::vector<base::OnceClosure> callbacks;
  std::swap(on_chrome_signin_dialog_completed_, callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void IdentityAPI::HandleSkipUIForTesting(base::OnceClosure on_complete) {
  is_chrome_signin_dialog_open_ = true;
  on_chrome_signin_dialog_completed_.push_back(std::move(on_complete));
  // Allow tests to complete the flow.
  std::move(skip_ui_for_testing_callback_)
      .Run(base::BindOnce(&IdentityAPI::OnChromeSigninDialogDestroyed,
                          weak_ptr_factory_.GetWeakPtr()));
  return;
}
#endif

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

void IdentityAPI::OnPrimaryAccountChanged(
    const PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      for (auto& account : GetAccountsWithRefreshTokensForExtensions()) {
        OnRefreshTokenUpdatedForAccount(account);
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      EraseStaleGaiaIdsForAllExtensions();
      base::flat_set<std::string> tracked_accounts;
      std::swap(tracked_accounts, accounts_known_to_extensions_);
      for (auto& account_id : tracked_accounts) {
        FireOnAccountSignInChanged(account_id, /*is_signed_in=*/false);
      }
      break;
  }
}

void IdentityAPI::OnRefreshTokensLoaded() {
  EraseStaleGaiaIdsForAllExtensions();
}

void IdentityAPI::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Refresh tokens are sometimes made available in contexts where
  // AccountTrackerService is not tracking the account in question. Bail out in
  // these cases.
  if (!HasAccessToChromeAccounts() || account_info.gaia.empty()) {
    return;
  }
  accounts_known_to_extensions_.insert(account_info.gaia);
  FireOnAccountSignInChanged(account_info.gaia, true);
}

void IdentityAPI::OnExtendedAccountInfoRemoved(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  EraseStaleGaiaIdsForAllExtensions();

  auto it = accounts_known_to_extensions_.find(account_info.gaia);
  if (it == accounts_known_to_extensions_.end()) {
    // Account unknown to Extensions.
    return;
  }
  accounts_known_to_extensions_.erase(it);
  FireOnAccountSignInChanged(account_info.gaia, false);
}

void IdentityAPI::FireOnAccountSignInChanged(const std::string& gaia_id,
                                             bool is_signed_in) {
  CHECK(!gaia_id.empty());
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
