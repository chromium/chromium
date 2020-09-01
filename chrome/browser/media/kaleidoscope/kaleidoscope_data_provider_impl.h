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
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class KaleidoscopeMetricsRecorder;
class Profile;

class KaleidoscopeDataProviderImpl
    : public media::mojom::KaleidoscopeDataProvider {
 public:
  KaleidoscopeDataProviderImpl(
      mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> receiver,
      Profile* profile,
      KaleidoscopeMetricsRecorder* metrics_recorder);
  KaleidoscopeDataProviderImpl(const KaleidoscopeDataProviderImpl&) = delete;
  KaleidoscopeDataProviderImpl& operator=(const KaleidoscopeDataProviderImpl&) =
      delete;
  ~KaleidoscopeDataProviderImpl() override;

  // media::mojom::KaleidoscopeDataProvider implementation.
  void GetTopMediaFeeds(media::mojom::KaleidoscopeTab tab,
                        GetTopMediaFeedsCallback callback) override;
  void GetMediaFeedContents(int64_t feed_id,
                            media::mojom::KaleidoscopeTab tab,
                            GetMediaFeedContentsCallback callback) override;
  void GetContinueWatchingMediaFeedItems(
      media::mojom::KaleidoscopeTab tab,
      GetContinueWatchingMediaFeedItemsCallback callback) override;
  void GetCredentials(GetCredentialsCallback cb) override;
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
  void GetHighWatchTimeOrigins(GetHighWatchTimeOriginsCallback cb) override;
  void SendFeedback() override;
  void GetCollections(const std::string& request,
                      GetCollectionsCallback cb) override;

 private:
  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  void OnGotCredentialsForCollections(const std::string& request,
                                      GetCollectionsCallback cb,
                                      media::mojom::CredentialsPtr credentials,
                                      media::mojom::CredentialsResult result);

  // Called when an access token request completes (successfully or not).
  void OnAccessTokenAvailable(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);

  void OnGotMediaFeedContents(
      GetMediaFeedContentsCallback callback,
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items);
  void OnGotContinueWatchingMediaFeedItems(
      GetContinueWatchingMediaFeedItemsCallback callback,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items);

  // Helper for fetching OAuth2 access tokens. This is non-null iff an access
  // token request is currently in progress.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // The current set of credentials.
  media::mojom::CredentialsPtr credentials_;

  // Pending credentials waiting on an access token.
  std::vector<GetCredentialsCallback> pending_callbacks_;

  signin::IdentityManager* identity_manager_;

  Profile* const profile_;

  KaleidoscopeMetricsRecorder* const metrics_recorder_;

  mojo::Receiver<media::mojom::KaleidoscopeDataProvider> receiver_;

  base::WeakPtrFactory<KaleidoscopeDataProviderImpl> weak_ptr_factory{this};
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_
