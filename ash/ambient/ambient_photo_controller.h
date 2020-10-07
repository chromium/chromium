// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// A wrapper class of SimpleURLLoader to download the photo raw data. In the
// test, this will be used to provide fake data.
class ASH_EXPORT AmbientURLLoader {
 public:
  AmbientURLLoader() = default;
  AmbientURLLoader(const AmbientURLLoader&) = delete;
  AmbientURLLoader& operator=(const AmbientURLLoader&) = delete;
  virtual ~AmbientURLLoader() = default;

  // Download data from the given |url|.
  virtual void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) = 0;

  virtual void DownloadToFile(
      const std::string& url,
      network::SimpleURLLoader::DownloadToFileCompleteCallback callback,
      const base::FilePath& file_path) = 0;
};

// A wrapper class of |data_decoder| to decode the photo raw data. In the test,
// this will be used to provide fake data.
class ASH_EXPORT AmbientImageDecoder {
 public:
  AmbientImageDecoder() = default;
  AmbientImageDecoder(const AmbientImageDecoder&) = delete;
  AmbientImageDecoder& operator=(const AmbientImageDecoder&) = delete;
  virtual ~AmbientImageDecoder() = default;

  // Decode |encoded_bytes| to ImageSkia.
  virtual void Decode(
      const std::vector<uint8_t>& encoded_bytes,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) = 0;
};

// Class to handle photos in ambient mode.
class ASH_EXPORT AmbientPhotoController : public AmbientBackendModelObserver {
 public:
  // Start fetching next |ScreenUpdate| from the backdrop server. The specified
  // download callback will be run upon completion and returns a null image
  // if: 1. the response did not have the desired fields or urls or, 2. the
  // download attempt from that url failed. The |icon_callback| also returns
  // the weather temperature in Fahrenheit together with the image.
  using TopicsDownloadCallback =
      base::OnceCallback<void(const std::vector<AmbientModeTopic>& topics)>;
  using WeatherIconDownloadCallback =
      base::OnceCallback<void(base::Optional<float>, const gfx::ImageSkia&)>;

  using PhotoDownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  AmbientPhotoController();
  ~AmbientPhotoController() override;

  // Start/stop updating the screen contents.
  // We need different logics to update photos and weather info because they
  // have different refreshing intervals. Currently we only update weather info
  // one time when entering ambient mode. Photos will be refreshed every
  // |kPhotoRefreshInterval|.
  void StartScreenUpdate();
  void StopScreenUpdate();

  void ScheduleFetchBackupImages();

  AmbientBackendModel* ambient_backend_model() {
    return &ambient_backend_model_;
  }

  const base::OneShotTimer& photo_refresh_timer_for_testing() const {
    return photo_refresh_timer_;
  }

  const base::OneShotTimer& backup_photo_refresh_timer_for_testing() const {
    return backup_photo_refresh_timer_;
  }

  // AmbientBackendModelObserver:
  void OnTopicsChanged() override;

  // Clear cache when Settings changes.
  void ClearCache();

 private:
  friend class AmbientAshTestBase;

  void FetchTopics();

  void FetchWeather();

  void ScheduleFetchTopics(bool backoff);

  void ScheduleRefreshImage();

  // Create the backup cache directory and start downloading images.
  void PrepareFetchBackupImages();

  // Download backup cache images.
  void FetchBackupImages();

  void OnBackupImageFetched(base::FilePath file_path);

  void GetScreenUpdateInfo();

  // Return a topic to download the image.
  // Return nullptr when need to read from disk cache.
  const AmbientModeTopic* GetNextTopic();

  void OnScreenUpdateInfoFetched(const ash::ScreenUpdate& screen_update);

  // Clear temporary image data to prepare next photos.
  void ResetImageData();

  // Fetch photo raw data by downloading or reading from cache.
  void FetchPhotoRawData();

  // Try to read photo raw data from cache.
  void TryReadPhotoRawData();

  void OnPhotoRawDataAvailable(bool from_downloading,
                               bool is_related_image,
                               base::RepeatingClosure on_done,
                               std::unique_ptr<std::string> details,
                               std::unique_ptr<std::string> data);

  void OnAllPhotoRawDataAvailable(bool from_downloading);

  void DecodePhotoRawData(bool from_downloading,
                          bool is_related_image,
                          base::RepeatingClosure on_done,
                          std::unique_ptr<std::string> data);

  void OnPhotoDecoded(bool from_downloading,
                      bool is_related_image,
                      base::RepeatingClosure on_done,
                      const gfx::ImageSkia& image);

  void OnAllPhotoDecoded(bool from_downloading, const std::string& hash);

  void StartDownloadingWeatherConditionIcon(
      const base::Optional<WeatherInfo>& weather_info);

  // Invoked upon completion of the weather icon download, |icon| can be a null
  // image if the download attempt from the url failed.
  void OnWeatherConditionIconDownloaded(float temp_f,
                                        bool show_celsius,
                                        const gfx::ImageSkia& icon);

  void set_url_loader_for_testing(
      std::unique_ptr<AmbientURLLoader> url_loader) {
    url_loader_ = std::move(url_loader);
  }

  AmbientURLLoader* get_url_loader_for_testing() { return url_loader_.get(); }

  void set_image_decoder_for_testing(
      std::unique_ptr<AmbientImageDecoder> image_decoder) {
    image_decoder_ = std::move(image_decoder);
  }

  AmbientImageDecoder* get_image_decoder_for_testing() {
    return image_decoder_.get();
  }

  void FetchTopicsForTesting();

  void FetchImageForTesting();

  void FetchBackupImagesForTesting();

  AmbientBackendModel ambient_backend_model_;

  // The timer to refresh photos.
  base::OneShotTimer photo_refresh_timer_;

  // The timer to refresh backup cache photos.
  base::OneShotTimer backup_photo_refresh_timer_;

  // The timer to refresh weather information.
  base::RepeatingTimer weather_refresh_timer_;

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

  // Whether the image refresh started or not.
  bool image_refresh_started_ = false;

  // Cached image may not exist or valid. This is the max times of attempts to
  // read cached images.
  int retries_to_read_from_cache_ = kMaxNumberOfCachedImages;

  int backup_retries_to_read_from_cache_ = 0;

  // Backoff for fetch topics retries.
  net::BackoffEntry fetch_topic_retry_backoff_;

  // Backoff to resume fetch images.
  net::BackoffEntry resume_fetch_image_backoff_;

  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};

  std::unique_ptr<AmbientURLLoader> url_loader_;

  std::unique_ptr<AmbientImageDecoder> image_decoder_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Temporary data store when fetching images and details.
  std::unique_ptr<std::string> image_data_;
  std::unique_ptr<std::string> related_image_data_;
  std::unique_ptr<std::string> image_details_;
  gfx::ImageSkia image_;
  gfx::ImageSkia related_image_;

  base::WeakPtrFactory<AmbientPhotoController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientPhotoController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
