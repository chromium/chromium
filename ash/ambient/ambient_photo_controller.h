// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientClient;
class AmbientAccessTokenController;

// Class to handle photos in ambient mode.
//
// Terminology:
//
// Topic - A primary and optional related photo specified by the IMAX server.
//
// Fetch Topics - Request new topics from the IMAX server. After they're
//                fetched, the controller just has urls for the primary/optional
//                photos in each returned topic.
//
// Download Topic - Download the encoded primary/related photos from their
//                  corresponding urls.
//
// Save Topic - Write the topic's encoded photos to disk for future re-use.
//              Helpful in future cases where ambient mode starts and there's no
//              internet.
//
// Load Topic - Read a previously saved topic's encoded photos from disk.
//
// Decode Topic - Decode the topic's photos and commit them to the
//                AmbientBackendModel.
//
// Prepare Topic - A term that aggregates all of the steps above:
// 1) Either a) fetch/download/save new topic or b) load existing topic
// 2) Decode topic and commit to the model.
//
// Topic Set - A group of topics for the UI to display in one cycle, capped by
// |AmbientPhotoConfig.topic_set_size|.
//
// The controller's state machine:
//
//        kInactive
//           |
//           |
//           v
// kPreparingNextTopicSet <-----
//           |                   |
//           |                   |
//           v                   |
// kWaitingForNextMarker -------
//
// kInactive:
// The controller is idle, and the model has no decoded topics in it. This is
// the initial state when the controller is constructed. Although not
// illustrated above, the controller can transition to this state from any of
// the other states via a call to StopScreenUpdate().
//
//
// kPreparingNextTopicSet (a.k.a. "refreshing" the model's topics):
// The very first time this state is reached, the UI has not started rendering
// yet, and the controller is preparing initial sets of topics. The
// AmbientPhotoConfig dictates how many sets to prepare initially. This state is
// initially triggered by a call to StartScreenUpdate(), and it ends when
// AmbientBackendModel::ImagesReady() is true.
//
// kWaitingForNextMarker:
// The UI is rendering the decoded topics currently in the model, and the
// controller is idle. It's waiting for the right marker(s) to be hit in the UI
// before it becomes active and starts preparing the next set of topics.
//
// kPreparingNextTopicSet (again):
// A target marker has been hit, and the controller immediately starts preparing
// the next set of topics. Unlike the first time this state was hit, there
// is only ever 1 topic set prepared, and the UI is rendering while the topics
// are being prepared. After the topic set is completely prepared, the
// controller goes back to WAITING_FOR_NEXT_MARKER. If another target marker is
// received while the controller is still preparing a topic set, the controller
// will simply reset its internal "counter" to 0 and start preparing a brand new
// set.
class ASH_EXPORT AmbientPhotoController : public AmbientBackendModelObserver,
                                          public AmbientViewEventHandler {
 public:
  AmbientPhotoController(AmbientClient& ambient_client,
                         AmbientAccessTokenController& access_token_controller,
                         AmbientPhotoConfig photo_config);

  AmbientPhotoController(const AmbientPhotoController&) = delete;
  AmbientPhotoController& operator=(const AmbientPhotoController&) = delete;

  ~AmbientPhotoController() override;

  // Start/stop updating the screen contents.
  // We need different logics to update photos and weather info because they
  // have different refreshing intervals.
  void StartScreenUpdate();
  void StopScreenUpdate();
  bool IsScreenUpdateActive() const;

  AmbientBackendModel* ambient_backend_model() {
    return &ambient_backend_model_;
  }

  base::OneShotTimer& backup_photo_refresh_timer_for_testing() {
    return backup_photo_refresh_timer_;
  }

  // AmbientBackendModelObserver:
  void OnTopicsChanged() override;

  // AmbientViewEventHandler:
  void OnMarkerHit(AmbientPhotoConfig::Marker marker) override;

  // Clear cache when Settings changes.
  void ClearCache();

 private:
  enum class State { kInactive, kWaitingForNextMarker, kPreparingNextTopicSet };

  // Describes the 2 cases for when new topics are fetched from the IMAX sever.
  enum class FetchTopicRequestType {
    // The controller is in the |kPreparingNextTopicSet| state and hence, there
    // is an immediate demand to prepare more topics. If it has exhausted all of
    // the existing topics in the model and the model has capacity to store more
    // topics, the controller will fetch a new set of topics and will
    // immediately download/save/decode topics from the new set afterwards.
    kOnDemand,
    // The controller can be in any state other than |kInactive|. It will fetch
    // a new set of topics if the |kTopicFetchInterval| has elapsed since the
    // last topic fetch completed, regardless of that fetch's request type. This
    // is done to guarantee that a sufficient amount of topics are available in
    // the model before the access token required to fetch new topics from the
    // server expires. In other words, a topic fetch is guaranteed to occur at
    // least every |kTopicFetchInterval| seconds until
    // |kMaxNumberOfCachedImages| topics are available in the model.
    //
    // Unlike an on-demand fetch, when a scheduled topic fetch completes, the
    // controller does *not* download/save/decode the topics from the new set
    // immediately after, regardless of the its state. The fetched topics are
    // only added to the model for future use.
    kScheduled
  };

  friend class AmbientAshTestBase;
  friend class AmbientPhotoControllerTest;
  friend std::ostream& operator<<(std::ostream& os, State state);

  // Initialize variables.
  void Init();

  void FetchTopics(FetchTopicRequestType request_type);

  void FetchWeather();

  void ScheduleFetchTopics(bool backoff);

  void ScheduleFetchBackupImages();

  // Download backup cache images.
  void FetchBackupImages();

  void OnBackupImageFetched(bool success);

  void GetScreenUpdateInfo();

  // Return a topic to download the image.
  // Return nullptr when need to read from disk cache.
  const AmbientModeTopic* GetNextTopic();

  void OnScreenUpdateInfoFetched(FetchTopicRequestType request_type,
                                 const ash::ScreenUpdate& screen_update);

  // Clear temporary image data to prepare next photos.
  void ResetImageData();

  // Fetch photo raw data by downloading or reading from cache.
  void FetchPhotoRawData();

  // Try to read photo raw data from cache.
  void TryReadPhotoRawData();

  void OnPhotoRawDataDownloaded(bool is_related_image,
                                base::RepeatingClosure on_done,
                                std::string&& data);

  void OnAllPhotoRawDataDownloaded();

  void OnAllPhotoRawDataAvailable(bool from_downloading);

  void OnPhotoRawDataSaved(bool from_downloading);

  void DecodePhotoRawData(bool from_downloading,
                          bool is_related_image,
                          base::RepeatingClosure on_done,
                          const std::string& data);

  void OnPhotoDecoded(bool from_downloading,
                      bool is_related_image,
                      base::RepeatingClosure on_done,
                      const gfx::ImageSkia& image);

  void OnAllPhotoDecoded(bool from_downloading,
                         const std::string& hash);

  void StartDownloadingWeatherConditionIcon(
      const absl::optional<WeatherInfo>& weather_info);

  // Invoked upon completion of the weather icon download, |icon| can be a null
  // image if the download attempt from the url failed.
  void OnWeatherConditionIconDownloaded(float temp_f,
                                        bool show_celsius,
                                        const gfx::ImageSkia& icon);

  void set_photo_cache_for_testing(
      std::unique_ptr<AmbientPhotoCache> photo_cache) {
    photo_cache_ = std::move(photo_cache);
  }

  AmbientPhotoCache* get_photo_cache_for_testing() {
    return photo_cache_.get();
  }

  void set_backup_photo_cache_for_testing(
      std::unique_ptr<AmbientPhotoCache> photo_cache) {
    backup_photo_cache_ = std::move(photo_cache);
  }

  AmbientPhotoCache* get_backup_photo_cache_for_testing() {
    return backup_photo_cache_.get();
  }

  void FetchTopicsForTesting();

  void FetchImageForTesting();

  void FetchBackupImagesForTesting();

  // Whether or not the controller has iterated through all of the topics that
  // are currently available in the model.
  bool HasExhaustedAllTopicsInModel() const;

  // Whether or not the model has any more room to store new topics fetched
  // from the server.
  bool HasModelReachedMaxTopicCapacity() const;

  // Kicks off preparation of the next topic.
  void StartPreparingNextTopic();

  AmbientBackendModel ambient_backend_model_;

  // The timer to refresh backup cache photos.
  base::OneShotTimer backup_photo_refresh_timer_;

  // The timer to refresh weather information.
  base::RepeatingTimer weather_refresh_timer_;

  State state_ = State::kInactive;

  // The index of a topic to download.
  size_t topic_index_ = 0;

  // Current index of cached image to read and display when failure happens.
  // The image file of this index may not exist or may not be valid. It will try
  // to read from the next cached file by increasing this index by 1.
  int cache_index_for_display_ = 0;

  // Current index of backup cached image to display when no other cached images
  // are available.
  size_t backup_cache_index_for_display_ = 0;

  // Current index of cached image to save for the latest downloaded photo.
  // The write command could fail. This index will increase 1 no matter writing
  // successfully or not. But theoretically we could not to change this index if
  // failures happen.
  int cache_index_for_store_ = 0;

  // Cached image may not exist or valid. This is the max times of attempts to
  // read cached images.
  int retries_to_read_from_cache_ = kMaxNumberOfCachedImages;

  int backup_retries_to_read_from_cache_ = 0;

  base::OneShotTimer fetch_topic_timer_;

  // Backoff for fetch topics retries.
  net::BackoffEntry fetch_topic_retry_backoff_;

  // Backoff to resume fetch images.
  net::BackoffEntry resume_fetch_image_backoff_;

  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observation_{this};

  std::unique_ptr<AmbientPhotoCache> photo_cache_;
  std::unique_ptr<AmbientPhotoCache> backup_photo_cache_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Temporary data store when fetching images and details.
  ::ambient::PhotoCacheEntry cache_entry_;
  gfx::ImageSkia image_;
  gfx::ImageSkia related_image_;

  // Tracks the number of topics that have been prepared since the controller
  // last transitioned to the |kPreparingNextTopicSet| state.
  size_t num_topics_prepared_ = 0;

  // Transient variable. Type of the most recent successful topic fetch.
  FetchTopicRequestType latest_fetch_topic_request_type_;

  base::WeakPtrFactory<AmbientPhotoController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
