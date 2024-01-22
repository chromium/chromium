// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"

#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/essential_search/socs_cookie_fetcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace app_list {
namespace {
bool IsEssentialSearchEnabled(PrefService* prefs) {
  return chromeos::features::IsEssentialSearchEnabled() &&
         prefs->GetBoolean(prefs::kSearchSuggestEnabled);
}
}  // namespace

EssentialSearchManager::EssentialSearchManager(Profile* primary_profile)
    : primary_profile_(primary_profile) {
  DCHECK(primary_profile_);
  auto* session_controller = ash::SessionController::Get();
  CHECK(session_controller);
  scoped_observation_.Observe(session_controller);
}

EssentialSearchManager::~EssentialSearchManager() = default;

// static
std::unique_ptr<EssentialSearchManager> EssentialSearchManager::Create(
    Profile* primary_profile) {
  return std::make_unique<EssentialSearchManager>(primary_profile);
}

void EssentialSearchManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state == session_manager::SessionState::ACTIVE &&
      IsEssentialSearchEnabled(primary_profile_->GetPrefs())) {
    FetchSocsCookie();
  }
}

void EssentialSearchManager::FetchSocsCookie() {
  socs_cookie_fetcher_ = std::make_unique<SocsCookieFetcher>(
      primary_profile_->GetURLLoaderFactory(), this);
  socs_cookie_fetcher_->StartFetching();
}

void EssentialSearchManager::OnCookieFetched(const std::string& cookie_header) {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();

  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      google_url, cookie_header, base::Time::Now(),
      std::nullopt /* server_time */, std::nullopt /* cookie_partition_key */));

  if (!cc) {
    LOG(ERROR) << "Invalid cookie header";
    OnApiCallFailed(SocsCookieFetcher::Status::kInvalidCookie);
    return;
  }

  const net::CookieOptions options = net::CookieOptions::MakeAllInclusive();

  primary_profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetCanonicalCookie(
          *cc, google_url, options,
          network::mojom::CookieManager::SetCanonicalCookieCallback());
}

void EssentialSearchManager::OnApiCallFailed(SocsCookieFetcher::Status status) {
  NOTIMPLEMENTED();
}

}  // namespace app_list
