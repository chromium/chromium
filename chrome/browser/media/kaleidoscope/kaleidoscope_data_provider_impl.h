// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/origin.h"

namespace media_history {
class MediaHistoryKeyedService;
}  // namespace media_history

namespace signin {
class IdentityManager;
}  // namespace signin

class KaleidoscopeMetricsRecorder;
class Profile;

class KaleidoscopeDataProviderImpl
    : public media::mojom::KaleidoscopeDataProvider,
      public media::mojom::KaleidoscopeNTPDataProvider {
 public:
  KaleidoscopeDataProviderImpl(
      mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> receiver,
      Profile* profile,
      KaleidoscopeMetricsRecorder* metrics_recorder);
  KaleidoscopeDataProviderImpl(
      mojo::PendingReceiver<media::mojom::KaleidoscopeNTPDataProvider> receiver,
      Profile* profile,
      KaleidoscopeMetricsRecorder* metrics_recorder);

  KaleidoscopeDataProviderImpl(const KaleidoscopeDataProviderImpl&) = delete;
  KaleidoscopeDataProviderImpl& operator=(const KaleidoscopeDataProviderImpl&) =
      delete;
  ~KaleidoscopeDataProviderImpl() override;

  // media::mojom::KaleidoscopeDataProvider implementation.
  void GetTopMediaFeeds(
      media::mojom::KaleidoscopeTab tab,
      media::mojom::KaleidoscopeDataProvider::GetTopMediaFeedsCallback callback)
      override;
  void GetMediaFeedContents(
      int64_t feed_id,
      media::mojom::KaleidoscopeTab tab,
      media::mojom::KaleidoscopeDataProvider::GetMediaFeedContentsCallback
          callback) override;
  void GetContinueWatchingMediaFeedItems(
      media::mojom::KaleidoscopeTab tab,
      media::mojom::KaleidoscopeDataProvider::
          GetContinueWatchingMediaFeedItemsCallback callback) override;
  void GetShouldShowFirstRunExperience(
      GetShouldShowFirstRunExperienceCallback cb) override;
  void SetFirstRunExperienceStep(
      media::mojom::KaleidoscopeFirstRunExperienceStep step) override;
  void GetAllMediaFeeds(GetAllMediaFeedsCallback cb) override;
  void SetMediaFeedsConsent(
      bool accepted_media_feeds,
      bool accepted_auto_select_media_feeds,
      const std::vector<int64_t>& enabled_feed_ids,
      const std::vector<int64_t>& disabled_feed_ids) override;
  void GetAutoSelectMediaFeedsConsent(
      GetAutoSelectMediaFeedsConsentCallback cb) override;
  void GetHighWatchTimeOrigins(GetHighWatchTimeOriginsCallback cb) override;
  void SendFeedback() override;
  void GetCollections(media::mojom::CredentialsPtr credentials,
                      const std::string& request,
                      GetCollectionsCallback cb) override;
  void GetSignedOutProviders(
      media::mojom::KaleidoscopeDataProvider::GetSignedOutProvidersCallback cb)
      override;
  void SetSignedOutProviders(
      const std::vector<std::string>& providers) override;
  void RecordTimeTakenToStartWatchHistogram(base::TimeDelta time) override;
  void RecordDialogClosedHistogram(bool value) override;
  void GetNewMediaFeeds(GetNewMediaFeedsCallback cb) override;
  void UpdateFeedUserStatus(int64_t feed_id,
                            media_feeds::mojom::FeedUserStatus status) override;

 private:
  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  KaleidoscopeDataProviderImpl(Profile* profile,
                               KaleidoscopeMetricsRecorder* metrics_recorder);

  void OnGotMediaFeedContents(
      GetMediaFeedContentsCallback callback,
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items);
  void OnGotContinueWatchingMediaFeedItems(
      media::mojom::KaleidoscopeDataProvider::
          GetContinueWatchingMediaFeedItemsCallback callback,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items);

  signin::IdentityManager* identity_manager_;

  Profile* const profile_;

  KaleidoscopeMetricsRecorder* const metrics_recorder_;

  mojo::Receiver<media::mojom::KaleidoscopeDataProvider> receiver_;

  mojo::Receiver<media::mojom::KaleidoscopeNTPDataProvider> ntp_receiver_;

  base::WeakPtrFactory<KaleidoscopeDataProviderImpl> weak_ptr_factory{this};
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_
