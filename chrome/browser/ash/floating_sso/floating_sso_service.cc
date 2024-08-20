// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_conversions.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "chrome/common/pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_util.h"

namespace ash::floating_sso {

namespace {

bool IsGoogleCookie(const net::CanonicalCookie& cookie) {
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      cookie.Domain(), cookie.SecureAttribute());

  return google_util::IsGoogleDomainUrl(
             cookie_domain_url, google_util::ALLOW_SUBDOMAIN,
             google_util::ALLOW_NON_STANDARD_PORTS) ||
         google_util::IsYoutubeDomainUrl(cookie_domain_url,
                                         google_util::ALLOW_SUBDOMAIN,
                                         google_util::ALLOW_NON_STANDARD_PORTS);
}

}  // namespace

FloatingSsoService::FloatingSsoService(
    PrefService* prefs,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    network::mojom::CookieManager* cookie_manager,
    syncer::OnceDataTypeStoreFactory create_store_callback)
    : prefs_(prefs),
      cookie_manager_(cookie_manager),
      bridge_(std::move(change_processor), std::move(create_store_callback)),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(
      ::prefs::kFloatingSsoEnabled,
      base::BindRepeating(&FloatingSsoService::StartOrStop,
                          base::Unretained(this)));
  StartOrStop();
}

void FloatingSsoService::StartOrStop() {
  // TODO: b/346354255 - subscribe to cookie changes to commit them to Sync when
  // needed. Remove `is_enabled_for_testing_` after we can can observe
  // meaningful behavior in tests.
  is_enabled_for_testing_ = prefs_->FindPreference(::prefs::kFloatingSsoEnabled)
                                ->GetValue()
                                ->GetBool();

  // TODO(b/355613655): stop listening for cookie changes when cookie sync gets
  // disabled.
  if (is_enabled_for_testing_) {
    MaybeStartListening();
  }
}

FloatingSsoService::~FloatingSsoService() = default;

void FloatingSsoService::Shutdown() {
  pref_change_registrar_.reset();
  prefs_ = nullptr;
}

void FloatingSsoService::MaybeStartListening() {
  if (!cookie_manager_) {
    return;
  }

  if (!receiver_.is_bound()) {
    BindToCookieManager();
  }
}

void FloatingSsoService::BindToCookieManager() {
  cookie_manager_->AddGlobalChangeListener(
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &FloatingSsoService::OnConnectionError, base::Unretained(this)));

  if (is_initial_cookie_manager_bind_) {
    cookie_manager_->GetAllCookies(base::BindOnce(
        &FloatingSsoService::OnCookiesLoaded, base::Unretained(this)));
  }
}

void FloatingSsoService::OnCookieChange(const net::CookieChangeInfo& change) {
  if (!ShouldSyncCookie(change.cookie)) {
    return;
  }
  std::optional<sync_pb::CookieSpecifics> sync_specifics =
      ToSyncProto(change.cookie);
  if (!sync_specifics.has_value()) {
    return;
  }

  switch (change.cause) {
    case net::CookieChangeCause::INSERTED:
      bridge_.AddOrUpdateCookie(sync_specifics.value());
      break;
    // All cases below correspond to deletion of a cookie. When intention is to
    // update the cookie (e.g. in the case of `CookieChangeCause::OVERWRITE`),
    // they will be immediately followed by a `CookieChangeCause::INSERTED`
    // change.
    case net::CookieChangeCause::EXPLICIT:
    case net::CookieChangeCause::UNKNOWN_DELETION:
    case net::CookieChangeCause::OVERWRITE:
    case net::CookieChangeCause::EXPIRED:
    case net::CookieChangeCause::EVICTED:
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      bridge_.DeleteCookie(sync_specifics.value().unique_key());
      break;
  }
}

void FloatingSsoService::OnCookiesLoaded(const net::CookieList& cookies) {
  for (const net::CanonicalCookie& cookie : cookies) {
    if (!ShouldSyncCookie(cookie)) {
      continue;
    }
    std::optional<sync_pb::CookieSpecifics> sync_specifics =
        ToSyncProto(cookie);
    if (!sync_specifics.has_value()) {
      continue;
    }
    bridge_.AddOrUpdateCookie(sync_specifics.value());
  }
}

bool FloatingSsoService::ShouldSyncCookie(
    const net::CanonicalCookie& cookie) const {
  // TODO(b/346354979): Respect kFloatingSsoDomainBlocklist and
  // kFloatingSsoDomainBlocklistExceptions policies.

  // Filter out session cookies (except when Floating Workspace is enabled).
  if (!cookie.IsPersistent() && !IsFloatingWorkspaceEnabled()) {
    return false;
  }

  // Filter out Google cookies.
  if (IsGoogleCookie(cookie)) {
    return false;
  }

  return true;
}

bool FloatingSsoService::IsFloatingWorkspaceEnabled() const {
  bool floating_workspace_policy_enabled =
      prefs_->GetBoolean(ash::prefs::kFloatingWorkspaceV2Enabled);
  return floating_workspace_policy_enabled &&
         ash::features::IsFloatingWorkspaceV2Enabled();
}

void FloatingSsoService::OnConnectionError() {
  is_initial_cookie_manager_bind_ = false;
  receiver_.reset();
  MaybeStartListening();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FloatingSsoService::GetControllerDelegate() {
  return bridge_.change_processor()->GetControllerDelegate();
}

}  // namespace ash::floating_sso
