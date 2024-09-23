// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/essential_search/socs_cookie_fetcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace app_list {
namespace {
constexpr base::TimeDelta kOneDay = base::Days(1);
const char kCookieName[] = "SOCS";

void LogStatus(SocsCookieFetcher::Status status) {
  base::UmaHistogramEnumeration("Ash.EssentialSearch.Status", status);
}

}  // namespace

const net::BackoffEntry::Policy
    EssentialSearchManager::kFetchSocsCookieRetryBackoffPolicy = {
        0,               // Number of initial errors to ignore.
        10 * 1000,       // Initial request delay in ms.
        2.0,             // Factor by which the waiting time will be multiplied.
        0.1,             // Fuzzing percentage.
        60 * 60 * 1000,  // Maximum request delay in ms.
        -1,              // Never discard the entry.
        true,            // Use initial delay in the first retry.
};

EssentialSearchManager::EssentialSearchManager(Profile* primary_profile)
    : primary_profile_(primary_profile),
      retry_backoff_(&kFetchSocsCookieRetryBackoffPolicy) {
  DCHECK(primary_profile_);
  auto* session_controller = ash::SessionController::Get();
  if (!session_controller) {
    CHECK_IS_TEST();
  } else {
    CHECK(session_controller);
    session_controller->AddObserver(this);
  }

  // Listen to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(primary_profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kEssentialSearchEnabled,
      base::BindRepeating(&EssentialSearchManager::MaybeFetchSocsCookie,
                          weak_ptr_factory_.GetWeakPtr()));

  // Handle the case where EssentialSearchManager is initialized after the
  // session was started.
  if (!session_manager::SessionManager::Get()) {
    CHECK_IS_TEST();
  } else if (session_manager::SessionManager::Get()->IsSessionStarted()) {
    MaybeFetchSocsCookie();
  }
}

EssentialSearchManager::~EssentialSearchManager() {
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->RemoveObserver(this);
  }
}

// static
std::unique_ptr<EssentialSearchManager> EssentialSearchManager::Create(
    Profile* primary_profile) {
  return std::make_unique<EssentialSearchManager>(primary_profile);
}

void EssentialSearchManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  // EssentialSearchManager only update SOCS cookie when user sign in/unlock the
  // device.
  if (state != session_manager::SessionState::ACTIVE) {
    return;
  }

  MaybeFetchSocsCookie();
}

void EssentialSearchManager::MaybeFetchSocsCookie() {
  // Check whether the feature flag is enabled.
  if (!chromeos::features::IsEssentialSearchEnabled()) {
    return;
  }

  PrefService* prefs = primary_profile_->GetPrefs();
  if (prefs->GetBoolean(prefs::kEssentialSearchEnabled)) {
    // If the policy is enabled, cancel all active requests then fetch SOCS
    // cookie.
    MaybeDisableSearchSuggest();
    CancelPendingRequests();
    socs_cookie_fetcher_ = std::make_unique<SocsCookieFetcher>(
        primary_profile_->GetURLLoaderFactory(), this);
    socs_cookie_fetcher_->StartFetching();
    return;
  }

  if (prefs->GetBoolean(prefs::kLastEssentialSearchValue)) {
    // If the policy was disabled, remove SOCS cookie from the user's profile.
    RemoveSocsCookie();
    return;
  }
}

void EssentialSearchManager::MaybeDisableSearchSuggest() {
  // Disable search suggest if last kEssentialSearchEnabled value was false
  // (until SOCS is fetched)
  if (!primary_profile_->GetPrefs()->GetBoolean(
          prefs::kLastEssentialSearchValue)) {
    temporary_disable_search_suggest_ = true;
    return;
  }

  // Check user profile for valid SOCS cookie
  primary_profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(
          GaiaUrls::GetInstance()->secure_google_url(),
          net::CookieOptions::MakeAllInclusive(),
          net::CookiePartitionKeyCollection(),
          base::BindOnce(&EssentialSearchManager::OnCookiesRetrieved,
                         weak_ptr_factory_.GetWeakPtr()));
}

void EssentialSearchManager::OnCookiesRetrieved(
    const net::CookieAccessResultList& list,
    const net::CookieAccessResultList& excluded_list) {
  // Assume disabled until valid SOCS found
  bool should_disable_search_suggest = true;
  for (const auto& cookie_with_access_result : list) {
    if (cookie_with_access_result.cookie.Name() == kCookieName) {
      should_disable_search_suggest = false;
      break;
    }
  }

  temporary_disable_search_suggest_ = should_disable_search_suggest;
}

bool EssentialSearchManager::ShouldDisableSearchSuggest() const {
  // TODO(b/312542928): collect UMA with the return type.
  return temporary_disable_search_suggest_;
}

void EssentialSearchManager::RemoveSocsCookie() {
  // Before removing the cookie, cancel all active request
  CancelPendingRequests();
  socs_cookie_fetcher_.reset();

  network::mojom::CookieDeletionFilterPtr filter =
      network::mojom::CookieDeletionFilter::New();

  filter->cookie_name = kCookieName;
  filter->url = GaiaUrls::GetInstance()->secure_google_url();

  primary_profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->DeleteCookies(std::move(filter),
                      base::BindOnce(&EssentialSearchManager::OnCookieDeleted,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void EssentialSearchManager::OnCookieDeleted(
    uint32_t number_of_cookies_deleted) {
  // Notify the test that OnCookieDeleted has been triggered.
  if (cookie_deletion_closure_for_test_) {
    CHECK_IS_TEST();
    std::move(cookie_deletion_closure_for_test_).Run();
  }

  if (number_of_cookies_deleted != 1) {
    LOG(WARNING) << "Failed to remove SOCS cookie";
  }

  // Update kLastEssentialSearchValue to avoid deleting SOCS cookie again.
  primary_profile_->GetPrefs()->SetBoolean(prefs::kLastEssentialSearchValue,
                                           false);
}

void EssentialSearchManager::OnCookieFetched(const std::string& cookie_header) {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();

  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      google_url, cookie_header, base::Time::Now(),
      std::nullopt /* server_time */, std::nullopt /* cookie_partition_key */,
      net::CookieSourceType::kOther,
      /*status=*/nullptr));

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
          base::BindOnce(&EssentialSearchManager::OnCookieAddedToUserProfile,
                         weak_ptr_factory_.GetWeakPtr()));
}

void EssentialSearchManager::OnCookieAddedToUserProfile(
    net::CookieAccessResult result) {
  // Notify the test that OnCookieAddedToUserProfile has been triggered.
  if (cookie_insertion_closure_for_test_) {
    CHECK_IS_TEST();
    std::move(cookie_insertion_closure_for_test_).Run();
  }

  if (!result.status.IsInclude()) {
    // Handle SOCS cookie insertion failure.
    LOG(WARNING) << "Failed to add SOCS cookie to user profile. Retrying.";
    LOG(WARNING) << "Status=" << result.status;
    OnApiCallFailed(SocsCookieFetcher::Status::kCookieInsertionFailure);
    return;
  }

  // Log success of fetching the cookie and adding it to the user profile.
  LogStatus(SocsCookieFetcher::Status::kOk);

  // After the SOCS cookie is added to the user profile, Schedule a SOCS cookie
  // refresh to ensure a valid SOCS cookie is maintained.
  temporary_disable_search_suggest_ = false;
  RefetchAfter(kOneDay);
  retry_backoff_.InformOfRequest(true);
  PrefService* prefs = primary_profile_->GetPrefs();
  if (prefs->GetBoolean(prefs::kEssentialSearchEnabled)) {
    // If the kEssentialSearchEnabled policy is still enabled, mark
    // kLastEssentialSearchValue as enabled.
    prefs->SetBoolean(prefs::kLastEssentialSearchValue, true);
  } else {
    // This would be triggered in case the policy got disabled while storing the
    // cookie.
    RemoveSocsCookie();
  }
}

void EssentialSearchManager::RefetchAfter(base::TimeDelta delay) {
  CancelPendingRequests();

  // Create new request after given delay
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EssentialSearchManager::MaybeFetchSocsCookie,
                     fetch_requests_weak_factory_.GetWeakPtr()),
      delay);
}

void EssentialSearchManager::CancelPendingRequests() {
  fetch_requests_weak_factory_.InvalidateWeakPtrs();
}

void EssentialSearchManager::OnApiCallFailed(SocsCookieFetcher::Status status) {
  LogStatus(status);
  retry_backoff_.InformOfRequest(false);
  RefetchAfter(retry_backoff_.GetTimeUntilRelease());
}

}  // namespace app_list
