// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_data_provider_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
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
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "media/base/media_switches.h"

namespace {

// The number of top media feeds to load for potential display.
constexpr unsigned kMediaFeedsLoadLimit = 5u;

// The minimum number of items a media feed needs to be displayed. This is the
// number of items needed to populate a collection.
constexpr int kMediaFeedsFetchedItemsMin = 4;

// The maximum number of feed items to display.
constexpr int kMediaFeedsItemsMaxCount = 20;

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
    : KaleidoscopeDataProviderImpl(profile, metrics_recorder) {
  receiver_.Bind(std::move(receiver));
}

KaleidoscopeDataProviderImpl::KaleidoscopeDataProviderImpl(
    mojo::PendingReceiver<media::mojom::KaleidoscopeNTPDataProvider> receiver,
    Profile* profile,
    KaleidoscopeMetricsRecorder* metrics_recorder)
    : KaleidoscopeDataProviderImpl(profile, metrics_recorder) {
  ntp_receiver_.Bind(std::move(receiver));
}

KaleidoscopeDataProviderImpl::KaleidoscopeDataProviderImpl(
    Profile* profile,
    KaleidoscopeMetricsRecorder* metrics_recorder)
    : profile_(profile),
      metrics_recorder_(metrics_recorder),
      receiver_(this),
      ntp_receiver_(this) {
  DCHECK(profile);

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
}

KaleidoscopeDataProviderImpl::~KaleidoscopeDataProviderImpl() = default;

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

void KaleidoscopeDataProviderImpl::GetAutoSelectMediaFeedsConsent(
    GetAutoSelectMediaFeedsConsentCallback cb) {
  auto* prefs = profile_->GetPrefs();
  if (!prefs) {
    std::move(cb).Run(false);
    return;
  }
  std::move(cb).Run(prefs->GetBoolean(
      kaleidoscope::prefs::kKaleidoscopeAutoSelectMediaFeeds));
}

void KaleidoscopeDataProviderImpl::GetHighWatchTimeOrigins(
    GetHighWatchTimeOriginsCallback cb) {
  GetMediaHistoryService()->GetHighWatchTimeOrigins(kProviderHighWatchTimeMin,
                                                    std::move(cb));
}

void KaleidoscopeDataProviderImpl::GetTopMediaFeeds(
    media::mojom::KaleidoscopeTab tab,
    media::mojom::KaleidoscopeDataProvider::GetTopMediaFeedsCallback callback) {
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
    media::mojom::KaleidoscopeDataProvider::GetMediaFeedContentsCallback
        callback) {
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
    media::mojom::KaleidoscopeDataProvider::
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
  chrome::ShowFeedbackPage(GURL(kKaleidoscopeWatchUIURL), profile_,
                           chrome::kFeedbackSourceKaleidoscope,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kKaleidoscopeFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);
}

void KaleidoscopeDataProviderImpl::GetCollections(
    media::mojom::CredentialsPtr credentials,
    const std::string& request,
    GetCollectionsCallback cb) {
  auto account_info = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  kaleidoscope::KaleidoscopeService::Get(profile_)->GetCollections(
      std::move(credentials), account_info.gaia, request, std::move(cb));
}

void KaleidoscopeDataProviderImpl::GetSignedOutProviders(
    media::mojom::KaleidoscopeDataProvider::GetSignedOutProvidersCallback cb) {
  DCHECK(!identity_manager_->HasPrimaryAccount(
      signin::ConsentLevel::kNotRequired));

  auto* providers = profile_->GetPrefs()->GetList(
      kaleidoscope::prefs::kKaleidoscopeSignedOutProviders);
  std::vector<std::string> providers_copy;
  for (auto& provider : providers->GetList()) {
    providers_copy.push_back(provider.GetString());
  }

  std::move(cb).Run(providers_copy);
}

void KaleidoscopeDataProviderImpl::SetSignedOutProviders(
    const std::vector<std::string>& providers) {
  DCHECK(!identity_manager_->HasPrimaryAccount(
      signin::ConsentLevel::kNotRequired));

  auto providers_copy = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (auto& provider : providers) {
    providers_copy->Append(provider);
  }

  profile_->GetPrefs()->Set(
      kaleidoscope::prefs::kKaleidoscopeSignedOutProviders, *providers_copy);
}

void KaleidoscopeDataProviderImpl::RecordTimeTakenToStartWatchHistogram(
    base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Media.Kaleidoscope.TimeTakenToStartWatch",
                                time);
}

void KaleidoscopeDataProviderImpl::RecordDialogClosedHistogram(bool value) {
  // If |value| is true then the user opened a watch action from this dialog.
  base::UmaHistogramBoolean("Media.Kaleidoscope.DialogClosed", value);
}

void KaleidoscopeDataProviderImpl::GetNewMediaFeeds(
    GetNewMediaFeedsCallback cb) {
  auto* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  const bool auto_enabled =
      prefs->GetBoolean(kaleidoscope::prefs::kKaleidoscopeAutoSelectMediaFeeds);

  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest::
          CreateNewFeeds(),
      base::BindOnce(
          [](GetNewMediaFeedsCallback cb, bool auto_enabled,
             std::vector<media_feeds::mojom::MediaFeedPtr> feeds) {
            std::move(cb).Run(std::move(feeds), auto_enabled);
          },
          std::move(cb), auto_enabled));
}

void KaleidoscopeDataProviderImpl::UpdateFeedUserStatus(
    int64_t feed_id,
    media_feeds::mojom::FeedUserStatus status) {
  GetMediaHistoryService()->UpdateFeedUserStatus(feed_id, status);
}

media_history::MediaHistoryKeyedService*
KaleidoscopeDataProviderImpl::GetMediaHistoryService() {
  return media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
      profile_);
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
    media::mojom::KaleidoscopeDataProvider::
        GetContinueWatchingMediaFeedItemsCallback callback,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items) {
  std::set<int64_t> ids;
  for (auto& item : items)
    ids.insert(item->id);

  // Mark the returned feed items as having been displayed.
  GetMediaHistoryService()->IncrementMediaFeedItemsShownCount(ids);

  std::move(callback).Run(std::move(items));
}
