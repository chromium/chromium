// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

// TODO(b/161357364): refactor utility functions and constants

constexpr net::BackoffEntry::Policy kResumeFetchImageBackoffPolicy = {
    kMaxConsecutiveReadPhotoFailures,  // Number of initial errors to ignore.
    500,                               // Initial delay in ms.
    2.0,            // Factor by which the waiting time will be multiplied.
    0.2,            // Fuzzing percentage.
    8 * 60 * 1000,  // Maximum delay in ms.
    -1,             // Never discard the entry.
    true,           // Use initial delay.
};

base::TaskTraits GetTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
}

const std::array<const char*, 2>& GetBackupPhotoUrls() {
  return Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->GetBackupPhotoUrls();
}

// Get the cache root path for ambient mode.
base::FilePath GetCacheRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
}

}  // namespace

AmbientPhotoController::AmbientPhotoController(
    AmbientClient& ambient_client,
    AmbientAccessTokenController& access_token_controller,
    AmbientViewDelegate& view_delegate,
    AmbientPhotoConfig photo_config)
    : ambient_backend_model_(std::move(photo_config)),
      resume_fetch_image_backoff_(&kResumeFetchImageBackoffPolicy),
      photo_cache_(AmbientPhotoCache::Create(
          GetCacheRootPath().Append(
              FILE_PATH_LITERAL(kAmbientModeCacheDirectoryName)),
          ambient_client,
          access_token_controller)),
      backup_photo_cache_(AmbientPhotoCache::Create(
          GetCacheRootPath().Append(
              FILE_PATH_LITERAL(kAmbientModeBackupCacheDirectoryName)),
          ambient_client,
          access_token_controller)),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits())) {
  scoped_view_delegate_observation_.Observe(&view_delegate);
  ScheduleFetchBackupImages();
}

AmbientPhotoController::~AmbientPhotoController() = default;

void AmbientPhotoController::Init(
    std::unique_ptr<AmbientTopicQueue::Delegate> topic_queue_delegate) {
  state_ = State::kPreparingNextTopicSet;
  topic_index_ = 0;
  retries_to_read_from_cache_ = kMaxNumberOfCachedImages;
  backup_retries_to_read_from_cache_ = GetBackupPhotoUrls().size();
  num_topics_prepared_ = 0;
  is_actively_preparing_topic_ = false;
  ambient_topic_queue_ = std::make_unique<AmbientTopicQueue>(
      /*topic_fetch_limit=*/kMaxNumberOfCachedImages,
      /*topic_fetch_size=*/kTopicsBatchSize, kTopicFetchInterval,
      ambient_backend_model_.photo_config().should_split_topics,
      std::move(topic_queue_delegate),
      Shell::Get()->ambient_controller()->ambient_backend_controller());
}

void AmbientPhotoController::StartScreenUpdate(
    std::unique_ptr<AmbientTopicQueue::Delegate> topic_queue_delegate) {
  if (state_ != State::kInactive) {
    DVLOG(3) << "AmbientPhotoController is already active. Ignoring "
                "StartScreenUpdate().";
    return;
  }

  Init(std::move(topic_queue_delegate));
  FetchWeather();
  weather_refresh_timer_.Start(
      FROM_HERE, kWeatherRefreshInterval,
      base::BindRepeating(&AmbientPhotoController::FetchWeather,
                          weak_factory_.GetWeakPtr()));
  if (backup_photo_refresh_timer_.IsRunning()) {
    // Would use |timer_.FireNow()| but this does not execute if screen is
    // locked. Manually call the expected callback instead.
    backup_photo_refresh_timer_.Stop();
    FetchBackupImages();
  }
  StartPreparingNextTopic();
}

void AmbientPhotoController::StopScreenUpdate() {
  state_ = State::kInactive;
  weather_refresh_timer_.Stop();
  resume_fetch_image_backoff_.Reset();
  ambient_backend_model_.Clear();
  ambient_topic_queue_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

bool AmbientPhotoController::IsScreenUpdateActive() const {
  return state_ != State::kInactive;
}

void AmbientPhotoController::OnMarkerHit(AmbientPhotoConfig::Marker marker) {
  if (!ambient_backend_model_.photo_config().refresh_topic_markers.contains(
          marker)) {
    DVLOG(3) << "UI event " << marker
             << " does not trigger a topic refresh. Ignoring...";
    return;
  }

  DVLOG(3) << "UI event " << marker << " triggering topic refresh";
  if (state_ == State::kInactive) {
    LOG(DFATAL) << "Received unexpected UI marker " << marker
                << " while inactive";
    return;
  }

  bool is_still_preparing_topics = state_ != State::kWaitingForNextMarker;
  state_ = State::kPreparingNextTopicSet;
  num_topics_prepared_ = 0;
  if (is_still_preparing_topics) {
    // The controller is still in the middle of preparing a topic from the
    // previous set (i.e. waiting on a callback or timer to fire). Resetting
    // |num_topics_prepared_| to 0 above is enough, and the topic currently
    // being prepared will count towards the next set.
    DVLOG(4) << "Did not finished preparing current topic set in time. "
                "Starting new set...";
  } else {
    StartPreparingNextTopic();
  }
}

void AmbientPhotoController::FetchWeather() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_weather_controller()
      ->FetchWeather();
}

void AmbientPhotoController::ClearCache() {
  DCHECK(photo_cache_);
  photo_cache_->Clear();
}

void AmbientPhotoController::ScheduleFetchBackupImages() {
  DVLOG(3) << __func__;
  if (backup_photo_refresh_timer_.IsRunning())
    return;

  backup_photo_refresh_timer_.Start(
      FROM_HERE,
      std::max(kBackupPhotoRefreshDelay,
               resume_fetch_image_backoff_.GetTimeUntilRelease()),
      base::BindOnce(&AmbientPhotoController::FetchBackupImages,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::FetchBackupImages() {
  const auto& backup_photo_urls = GetBackupPhotoUrls();
  backup_retries_to_read_from_cache_ = backup_photo_urls.size();
  for (size_t i = 0; i < backup_photo_urls.size(); i++) {
    backup_photo_cache_->DownloadPhotoToFile(
        backup_photo_urls.at(i),
        /*cache_index=*/i,
        base::BindOnce(&AmbientPhotoController::OnBackupImageFetched,
                       weak_factory_.GetWeakPtr()));
  }
}

void AmbientPhotoController::OnBackupImageFetched(bool success) {
  if (!success) {
    // TODO(b/169807068) Change to retry individual failed images.
    resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);
    LOG(WARNING) << "Downloading backup image failed.";
    ScheduleFetchBackupImages();
    return;
  }
  resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/true);
}

void AmbientPhotoController::OnTopicsAvailableInQueue(
    AmbientTopicQueue::WaitResult wait_result) {
  if (state_ != State::kPreparingNextTopicSet)
    return;

  switch (wait_result) {
    case AmbientTopicQueue::WaitResult::kTopicsAvailable:
      ReadPhotoFromTopicQueue();
      break;
    case AmbientTopicQueue::WaitResult::kTopicFetchBackingOff:
    case AmbientTopicQueue::WaitResult::kTopicFetchLimitReached:
      // If there are no topics in the queue, will try to read from disk cache.
      TryReadPhotoFromCache();
      break;
  }
}

void AmbientPhotoController::ResetImageData() {
  cache_entry_.Clear();

  image_ = gfx::ImageSkia();
  related_image_ = gfx::ImageSkia();
}

void AmbientPhotoController::ReadPhotoFromTopicQueue() {
  ResetImageData();
  DVLOG(3) << "Downloading topic photos";
  AmbientModeTopic topic = ambient_topic_queue_->Pop();
  ::ambient::Photo* photo = cache_entry_.mutable_primary_photo();
  photo->set_details(topic.details);
  photo->set_is_portrait(topic.is_portrait);
  photo->set_type(topic.topic_type);

  const int num_callbacks = (topic.related_image_url.empty()) ? 1 : 2;
  auto on_done = base::BarrierClosure(
      num_callbacks,
      base::BindOnce(&AmbientPhotoController::OnAllPhotoRawDataDownloaded,
                     weak_factory_.GetWeakPtr()));

  photo_cache_->DownloadPhoto(
      topic.url,
      base::BindOnce(&AmbientPhotoController::OnPhotoRawDataDownloaded,
                     weak_factory_.GetWeakPtr(),
                     /*is_related_image=*/false, on_done));

  if (!topic.related_image_url.empty()) {
    ::ambient::Photo* related_photo = cache_entry_.mutable_related_photo();
    related_photo->set_details(topic.related_details);
    related_photo->set_is_portrait(topic.is_portrait);
    related_photo->set_type(topic.topic_type);

    photo_cache_->DownloadPhoto(
        topic.related_image_url,
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataDownloaded,
                       weak_factory_.GetWeakPtr(),
                       /*is_related_image=*/true, on_done));
  }
}

void AmbientPhotoController::TryReadPhotoFromCache() {
  ResetImageData();
  // Stop reading from cache after the max number of retries.
  if (retries_to_read_from_cache_ == 0) {
    if (backup_retries_to_read_from_cache_ == 0) {
      LOG(WARNING) << "Failed to read from cache";
      is_actively_preparing_topic_ = false;
      ambient_backend_model_.AddImageFailure();
      // Do not refresh image if image loading has failed repeatedly, or there
      // are no more topics to retry. Note |ambient_topic_queue_| may be null
      // if AddImageFailure() ultimately led to an AmbientBackendModelObserver
      // calling StopScreenUpdate().
      if (ambient_backend_model_.ImageLoadingFailed() ||
          !ambient_topic_queue_ || ambient_topic_queue_->IsEmpty()) {
        LOG(WARNING) << "Not attempting image refresh";
        return;
      }

      // Try to resume normal workflow with backoff.
      const base::TimeDelta delay =
          resume_fetch_image_backoff_.GetTimeUntilRelease();
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AmbientPhotoController::StartPreparingNextTopic,
                         weak_factory_.GetWeakPtr()),
          delay);
      return;
    }

    --backup_retries_to_read_from_cache_;

    DVLOG(3) << "Read from backup cache index: "
             << backup_cache_index_for_display_;
    // Try to read a backup image.
    backup_photo_cache_->ReadPhotoCache(
        /*cache_index=*/backup_cache_index_for_display_,
        base::BindOnce(&AmbientPhotoController::OnPhotoCacheReadComplete,
                       weak_factory_.GetWeakPtr()));

    backup_cache_index_for_display_++;
    if (backup_cache_index_for_display_ == GetBackupPhotoUrls().size())
      backup_cache_index_for_display_ = 0;
    return;
  }

  --retries_to_read_from_cache_;
  int current_cache_index = cache_index_for_display_;

  ++cache_index_for_display_;
  if (cache_index_for_display_ == kMaxNumberOfCachedImages)
    cache_index_for_display_ = 0;

  DVLOG(3) << "Read from cache index: " << current_cache_index;
  photo_cache_->ReadPhotoCache(
      current_cache_index,
      base::BindOnce(&AmbientPhotoController::OnPhotoCacheReadComplete,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::OnPhotoCacheReadComplete(
    ::ambient::PhotoCacheEntry cache_entry) {
  cache_entry_ = std::move(cache_entry);
  OnAllPhotoRawDataAvailable(/*from_downloading=*/false);
}

void AmbientPhotoController::OnPhotoRawDataDownloaded(
    bool is_related_image,
    base::RepeatingClosure on_done,
    std::string&& data) {
  if (is_related_image)
    cache_entry_.mutable_related_photo()->set_image(std::move(data));
  else
    cache_entry_.mutable_primary_photo()->set_image(std::move(data));

  std::move(on_done).Run();
}

void AmbientPhotoController::OnAllPhotoRawDataDownloaded() {
  DVLOG(3) << __func__;
  OnAllPhotoRawDataAvailable(/*from_downloading=*/true);
}

void AmbientPhotoController::OnAllPhotoRawDataAvailable(bool from_downloading) {
  if (!cache_entry_.has_primary_photo() ||
      cache_entry_.primary_photo().image().empty()) {
    if (from_downloading) {
      LOG(ERROR) << "Failed to download image";
      resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);
    }
    // Try to read from cache when failure happens.
    TryReadPhotoFromCache();
    return;
  }

  if (from_downloading) {
    // If the data is fetched from downloading, write to disk.
    // Note: WritePhotoCache could fail. The saved file name may not be
    // continuous.
    DVLOG(3) << "Save photo to cache index: " << cache_index_for_store_;
    auto current_cache_index = cache_index_for_store_;
    ++cache_index_for_store_;
    if (cache_index_for_store_ == kMaxNumberOfCachedImages)
      cache_index_for_store_ = 0;

    photo_cache_->WritePhotoCache(
        /*cache_index=*/current_cache_index, cache_entry_,
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataSaved,
                       weak_factory_.GetWeakPtr(), from_downloading));
  } else {
    OnPhotoRawDataSaved(from_downloading);
  }
}

void AmbientPhotoController::OnPhotoRawDataSaved(bool from_downloading) {
  const bool has_related = cache_entry_.has_related_photo() &&
                           !cache_entry_.related_photo().image().empty();
  const int num_callbacks = has_related ? 2 : 1;

  auto on_done = base::BarrierClosure(
      num_callbacks,
      base::BindOnce(
          &AmbientPhotoController::OnAllPhotoDecoded,
          weak_factory_.GetWeakPtr(), from_downloading,
          /*hash=*/base::SHA1HashString(cache_entry_.primary_photo().image())));

  DecodePhotoRawData(from_downloading,
                     /*is_related_image=*/false, on_done,
                     cache_entry_.primary_photo().image());

  if (has_related) {
    DecodePhotoRawData(from_downloading, /*is_related_image=*/true, on_done,
                       cache_entry_.related_photo().image());
  }
}

void AmbientPhotoController::DecodePhotoRawData(bool from_downloading,
                                                bool is_related_image,
                                                base::RepeatingClosure on_done,
                                                const std::string& data) {
  photo_cache_->DecodePhoto(
      data, base::BindOnce(&AmbientPhotoController::OnPhotoDecoded,
                           weak_factory_.GetWeakPtr(), from_downloading,
                           is_related_image, std::move(on_done)));
}

void AmbientPhotoController::OnPhotoDecoded(bool from_downloading,
                                            bool is_related_image,
                                            base::RepeatingClosure on_done,
                                            const gfx::ImageSkia& image) {
  if (is_related_image)
    related_image_ = image;
  else
    image_ = image;

  std::move(on_done).Run();
}

void AmbientPhotoController::OnAllPhotoDecoded(bool from_downloading,
                                               const std::string& hash) {
  DVLOG(3) << __func__;
  DCHECK_EQ(state_, State::kPreparingNextTopicSet);
  DCHECK(is_actively_preparing_topic_);
  if (image_.isNull()) {
    LOG(WARNING) << "Image decoding failed";
    if (from_downloading)
      resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);

    // Try to read from cache when failure happens.
    TryReadPhotoFromCache();
    return;
  } else if (ambient_backend_model_.IsHashDuplicate(hash)) {
    LOG(WARNING) << "Skipping loading duplicate image.";
    TryReadPhotoFromCache();
    return;
  }

  is_actively_preparing_topic_ = false;
  retries_to_read_from_cache_ = kMaxNumberOfCachedImages;
  backup_retries_to_read_from_cache_ = GetBackupPhotoUrls().size();

  if (from_downloading)
    resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/true);

  PhotoWithDetails detailed_photo;
  detailed_photo.photo = image_;
  detailed_photo.related_photo = related_image_;
  detailed_photo.details = cache_entry_.primary_photo().details();
  detailed_photo.related_details = cache_entry_.related_photo().details();
  detailed_photo.is_portrait = cache_entry_.primary_photo().is_portrait();
  detailed_photo.topic_type = cache_entry_.primary_photo().type();
  detailed_photo.hash = hash;

  ResetImageData();

  size_t target_num_topics_to_prepare =
      ambient_backend_model_.ImagesReady()
          ? ambient_backend_model_.photo_config().topic_set_size
          : ambient_backend_model_.photo_config().GetNumDecodedTopicsToBuffer();
  // AddNextImage() can call out to observers, who can synchronously interact
  // with the controller again within their observer notification methods. So
  // the internal |state_| and |num_topics_prepared_| should be updated and
  // captured in local variables before calling AddNextImage(). This ensures
  // that the behavior and state of the controller is consistent with the model.
  ++num_topics_prepared_;
  bool more_topics_required =
      num_topics_prepared_ < target_num_topics_to_prepare;
  if (!more_topics_required) {
    state_ = State::kWaitingForNextMarker;
  }

  ambient_backend_model_.AddNextImage(std::move(detailed_photo));

  if (more_topics_required) {
    StartPreparingNextTopic();
  }
}

void AmbientPhotoController::FetchTopicsForTesting() {
  StartPreparingNextTopic();
}

void AmbientPhotoController::FetchImageForTesting() {
  is_actively_preparing_topic_ = true;
  if (!ambient_topic_queue_->IsEmpty()) {
    ReadPhotoFromTopicQueue();
  } else {
    TryReadPhotoFromCache();
  }
}

void AmbientPhotoController::FetchBackupImagesForTesting() {
  FetchBackupImages();
}

void AmbientPhotoController::StartPreparingNextTopic() {
  DCHECK_EQ(state_, State::kPreparingNextTopicSet);
  DCHECK(!is_actively_preparing_topic_)
      << "Preparing multiple topics simultaneously is not currently supported";
  is_actively_preparing_topic_ = true;
  ambient_topic_queue_->WaitForTopicsAvailable(
      base::BindOnce(&AmbientPhotoController::OnTopicsAvailableInQueue,
                     weak_factory_.GetWeakPtr()));
}

std::ostream& operator<<(std::ostream& os,
                         AmbientPhotoController::State state) {
  switch (state) {
    case AmbientPhotoController::State::kInactive:
      return os << "INACTIVE";
    case AmbientPhotoController::State::kWaitingForNextMarker:
      return os << "WAITING_FOR_NEXT_MARKER";
    case AmbientPhotoController::State::kPreparingNextTopicSet:
      return os << "PREPARING_NEXT_TOPIC_SET";
  }
}

}  // namespace ash
