// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_data_provider_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_metrics_recorder.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "media/base/media_switches.h"

namespace {

// The number of top media feeds to load for potential display.
constexpr unsigned kMediaFeedsLoadLimit = 5u;

// The minimum number of items a media feed needs to be displayed. This is the
// number of items needed to populate a collection.
constexpr int kMediaFeedsFetchedItemsMin = 4;

// The maximum number of feed items to display.
constexpr int kMediaFeedsItemsMaxCount = 20;

constexpr char kChromeMediaRecommendationsOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-media-recommendations";

// The minimum watch time needed in media history for a provider to be
// considered high watch time.
constexpr base::TimeDelta kProviderHighWatchTimeMin =
    base::TimeDelta::FromMinutes(30);

// The feedback tag for Kaleidoscope.
constexpr char kKaleidoscopeFeedbackCategoryTag[] =
    "kaleidoscope_settings_menu";

base::Optional<media_feeds::mojom::MediaFeedItemType> GetFeedItemTypeForTab(
    media::mojom::KaleidoscopeTab tab) {
  switch (tab) {
    case media::mojom::KaleidoscopeTab::kForYou:
      return base::nullopt;
    case media::mojom::KaleidoscopeTab::kMovies:
      return media_feeds::mojom::MediaFeedItemType::kMovie;
    case media::mojom::KaleidoscopeTab::kTVShows:
      return media_feeds::mojom::MediaFeedItemType::kTVSeries;
  }
}

}  // namespace

KaleidoscopeDataProviderImpl::KaleidoscopeDataProviderImpl(
    mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> receiver,
    Profile* profile,
    KaleidoscopeMetricsRecorder* metrics_recorder)
    : credentials_(media::mojom::Credentials::New()),
      profile_(profile),
      metrics_recorder_(metrics_recorder),
      receiver_(this, std::move(receiver)) {
  DCHECK(profile);

  // If this is Google Chrome then we should use the official API key.
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    credentials_->api_key = is_stable_channel
                                ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
}

KaleidoscopeDataProviderImpl::~KaleidoscopeDataProviderImpl() = default;

void KaleidoscopeDataProviderImpl::GetCredentials(GetCredentialsCallback cb) {
  // If the profile is incognito then disable Kaleidoscope.
  if (profile_->IsOffTheRecord()) {
    std::move(cb).Run(nullptr,
                      media::mojom::CredentialsResult::kFailedIncognito);
    return;
  }

  // If the profile is a child then disable Kaleidoscope.
  if (profile_->IsSupervised() || profile_->IsChild()) {
    std::move(cb).Run(nullptr, media::mojom::CredentialsResult::kFailedChild);
    return;
  }

  // If the administrator has disabled Kaleidoscope then stop.
  auto* prefs = profile_->GetPrefs();
  if (!prefs->GetBoolean(kaleidoscope::prefs::kKaleidoscopePolicyEnabled)) {
    std::move(cb).Run(nullptr,
                      media::mojom::CredentialsResult::kDisabledByPolicy);
    return;
  }

  // If the user is not signed in, return the credentials without an access
  // token. Sync consent is not required to use Kaleidoscope.
  if (!identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired)) {
    std::move(cb).Run(credentials_.Clone(),
                      media::mojom::CredentialsResult::kSuccess);
    return;
  }

  pending_callbacks_.push_back(std::move(cb));

  // Get an OAuth token for the backend API. This token will be limited to just
  // our backend scope. Destroying |token_fetcher_| will cancel the fetch so
  // unretained is safe here.
  signin::ScopeSet scopes = {kChromeMediaRecommendationsOAuth2Scope};
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "kaleidoscope_service", identity_manager_, scopes,
      base::BindOnce(&KaleidoscopeDataProviderImpl::OnAccessTokenAvailable,
                     base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kNotRequired);
}

void KaleidoscopeDataProviderImpl::GetShouldShowFirstRunExperience(
    GetShouldShowFirstRunExperienceCallback cb) {
  auto* service = kaleidoscope::KaleidoscopeService::Get(profile_);

  // The Kaleidoscope Service would not be available for incognito profiles.
  if (!service) {
    std::move(cb).Run(false);
    return;
  }

  std::move(cb).Run(service->ShouldShowFirstRunExperience());
}

void KaleidoscopeDataProviderImpl::SetFirstRunExperienceStep(
    media::mojom::KaleidoscopeFirstRunExperienceStep step) {
  if (metrics_recorder_)
    metrics_recorder_->OnFirstRunExperienceStepChanged(step);

  // If the FRE is completed, then store a pref so we know to not show it again.
  if (step != media::mojom::KaleidoscopeFirstRunExperienceStep::kCompleted)
    return;

  auto* prefs = profile_->GetPrefs();
  if (!prefs)
    return;

  prefs->SetInteger(kaleidoscope::prefs::kKaleidoscopeFirstRunCompleted,
                    kKaleidoscopeFirstRunLatestVersion);

  // Delete any cached data that has the first run stored in it.
  GetMediaHistoryService()->DeleteKaleidoscopeData();
}

void KaleidoscopeDataProviderImpl::GetAllMediaFeeds(
    GetAllMediaFeedsCallback cb) {
  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest(),
      std::move(cb));
}

void KaleidoscopeDataProviderImpl::SetMediaFeedsConsent(
    bool accepted_media_feeds,
    bool accepted_auto_select_media_feeds,
    const std::vector<int64_t>& enabled_feed_ids,
    const std::vector<int64_t>& disabled_feed_ids) {
  auto* prefs = profile_->GetPrefs();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kMediaFeedsBackgroundFetching, accepted_media_feeds);
  prefs->SetBoolean(prefs::kMediaFeedsSafeSearchEnabled, accepted_media_feeds);

  // If the user declined to use Media Feeds at all, then there's nothing left
  // to do.
  if (!accepted_media_feeds)
    return;

  prefs->SetBoolean(kaleidoscope::prefs::kKaleidoscopeAutoSelectMediaFeeds,
                    accepted_auto_select_media_feeds);

  for (auto& feed_id : disabled_feed_ids) {
    GetMediaHistoryService()->UpdateFeedUserStatus(
        feed_id, media_feeds::mojom::FeedUserStatus::kDisabled);
  }

  for (auto& feed_id : enabled_feed_ids) {
    GetMediaHistoryService()->UpdateFeedUserStatus(
        feed_id, media_feeds::mojom::FeedUserStatus::kEnabled);
  }
}

void KaleidoscopeDataProviderImpl::GetHighWatchTimeOrigins(
    GetHighWatchTimeOriginsCallback cb) {
  GetMediaHistoryService()->GetHighWatchTimeOrigins(kProviderHighWatchTimeMin,
                                                    std::move(cb));
}

void KaleidoscopeDataProviderImpl::GetTopMediaFeeds(
    media::mojom::KaleidoscopeTab tab,
    GetTopMediaFeedsCallback callback) {
  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest::
          CreateTopFeedsForDisplay(
              kMediaFeedsLoadLimit, kMediaFeedsFetchedItemsMin,
              // Require Safe Search checking if the integration is enabled.
              base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch),
              GetFeedItemTypeForTab(tab)),
      std::move(callback));
}

void KaleidoscopeDataProviderImpl::GetMediaFeedContents(
    int64_t feed_id,
    media::mojom::KaleidoscopeTab tab,
    GetMediaFeedContentsCallback callback) {
  GetMediaHistoryService()->GetMediaFeedItems(
      media_history::MediaHistoryKeyedService::GetMediaFeedItemsRequest::
          CreateItemsForFeed(
              feed_id, kMediaFeedsItemsMaxCount,
              // Require Safe Search checking if the integration is enabled.
              base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch),
              GetFeedItemTypeForTab(tab)),
      base::BindOnce(&KaleidoscopeDataProviderImpl::OnGotMediaFeedContents,
                     weak_ptr_factory.GetWeakPtr(), std::move(callback),
                     feed_id));
}

void KaleidoscopeDataProviderImpl::GetContinueWatchingMediaFeedItems(
    media::mojom::KaleidoscopeTab tab,
    GetContinueWatchingMediaFeedItemsCallback callback) {
  GetMediaHistoryService()->GetMediaFeedItems(
      media_history::MediaHistoryKeyedService::GetMediaFeedItemsRequest::
          CreateItemsForContinueWatching(
              kMediaFeedsItemsMaxCount,
              // Require Safe Search checking if the integration is enabled.
              base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch),
              GetFeedItemTypeForTab(tab)),
      base::BindOnce(
          &KaleidoscopeDataProviderImpl::OnGotContinueWatchingMediaFeedItems,
          weak_ptr_factory.GetWeakPtr(), std::move(callback)));
}

void KaleidoscopeDataProviderImpl::SendFeedback() {
  chrome::ShowFeedbackPage(GURL(kKaleidoscopeUIURL), profile_,
                           chrome::kFeedbackSourceKaleidoscope,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kKaleidoscopeFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);
}

void KaleidoscopeDataProviderImpl::GetCollections(const std::string& request,
                                                  GetCollectionsCallback cb) {
  GetCredentials(base::BindOnce(
      &KaleidoscopeDataProviderImpl::OnGotCredentialsForCollections,
      weak_ptr_factory.GetWeakPtr(), request, std::move(cb)));
}

void KaleidoscopeDataProviderImpl::OnGotCredentialsForCollections(
    const std::string& request,
    GetCollectionsCallback cb,
    media::mojom::CredentialsPtr credentials,
    media::mojom::CredentialsResult result) {
  // If we have no credentials then we should return an empty response.
  if (result != media::mojom::CredentialsResult::kSuccess) {
    std::move(cb).Run(media::mojom::GetCollectionsResponse::New(
        "", media::mojom::GetCollectionsResult::kFailed));
    return;
  }

  auto account_info = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  kaleidoscope::KaleidoscopeService::Get(profile_)->GetCollections(
      std::move(credentials), account_info.gaia, request, std::move(cb));
}

media_history::MediaHistoryKeyedService*
KaleidoscopeDataProviderImpl::GetMediaHistoryService() {
  return media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
      profile_);
}

void KaleidoscopeDataProviderImpl::OnAccessTokenAvailable(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::State::NONE) {
    credentials_->access_token = access_token_info.token;
    credentials_->expiry_time = access_token_info.expiration_time;
  }

  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run(credentials_.Clone(),
                            media::mojom::CredentialsResult::kSuccess);
  }

  pending_callbacks_.clear();
}

void KaleidoscopeDataProviderImpl::OnGotMediaFeedContents(
    GetMediaFeedContentsCallback callback,
    const int64_t feed_id,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items) {
  std::set<int64_t> ids;
  for (auto& item : items)
    ids.insert(item->id);

  // Mark the returned feed and feed items as having been displayed.
  GetMediaHistoryService()->UpdateMediaFeedDisplayTime(feed_id);
  GetMediaHistoryService()->IncrementMediaFeedItemsShownCount(ids);

  std::move(callback).Run(std::move(items));
}

void KaleidoscopeDataProviderImpl::OnGotContinueWatchingMediaFeedItems(
    GetContinueWatchingMediaFeedItemsCallback callback,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items) {
  std::set<int64_t> ids;
  for (auto& item : items)
    ids.insert(item->id);

  // Mark the returned feed items as having been displayed.
  GetMediaHistoryService()->IncrementMediaFeedItemsShownCount(ids);

  std::move(callback).Run(std::move(items));
}
