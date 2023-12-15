// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_backup_photo_downloader.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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

}  // namespace

AmbientPhotoController::AmbientPhotoController(
    AmbientViewDelegate& view_delegate,
    AmbientPhotoConfig photo_config,
    std::unique_ptr<AmbientTopicQueue::Delegate> topic_queue_delegate)
    : topic_queue_delegate_(std::move(topic_queue_delegate)),
      ambient_backend_model_(std::move(photo_config)),
      resume_fetch_image_backoff_(&kResumeFetchImageBackoffPolicy),
      access_token_controller_(
          Shell::Get()->ambient_controller()->access_token_controller()),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits())) {
  CHECK(topic_queue_delegate_);
  CHECK(access_token_controller_);
  scoped_view_delegate_observation_.Observe(&view_delegate);
  ScheduleFetchBackupImages();
}

AmbientPhotoController::~AmbientPhotoController() = default;

void AmbientPhotoController::Init() {
  state_ = State::kPreparingNextTopicSet;
  topic_index_ = 0;
  retries_to_read_from_cache_ = kMaxNumberOfCachedImages;
  backup_retries_to_read_from_cache_ = GetBackupPhotoUrls().size();
  num_topics_prepared_ = 0;
  is_actively_preparing_topic_ = false;
  ambient_topic_queue_ = std::make_unique<AmbientTopicQueue>(
      /*topic_fetch_limit=*/ambient_backend_model_.photo_config().IsEmpty()
          ? 0
          : kMaxNumberOfCachedImages,
      /*topic_fetch_size=*/kTopicsBatchSize, kTopicFetchInterval,
      ambient_backend_model_.photo_config().should_split_topics,
      topic_queue_delegate_.get(),
      Shell::Get()->ambient_controller()->ambient_backend_controller());
}

void AmbientPhotoController::StartScreenUpdate() {
  if (state_ != State::kInactive) {
    DVLOG(3) << "AmbientPhotoController is already active. Ignoring "
                "StartScreenUpdate().";
    return;
  }

  Init();
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
  switch (state_) {
    case State::kInactive:
      LOG(DFATAL) << "Received unexpected UI marker " << marker
                  << " while inactive";
      break;
    case State::kPreparingNextTopicSet:
      // The controller is still in the middle of preparing a topic from the
      // previous set (i.e. waiting on a callback or timer to fire). Resetting
      // |num_topics_prepared_| to 0 is enough, and the topic currently being
      // prepared will count towards the next set.
      DVLOG(4) << "Did not finished preparing current topic set in time. "
                  "Starting new set...";
      num_topics_prepared_ = 0;
      break;
    case State::kWaitingForNextMarker:
      state_ = State::kPreparingNextTopicSet;
      num_topics_prepared_ = 0;
      StartPreparingNextTopic();
      break;
  }
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
  active_backup_image_downloads_.clear();
  const auto& backup_photo_urls = GetBackupPhotoUrls();
  backup_retries_to_read_from_cache_ = backup_photo_urls.size();
  const std::vector<gfx::Size> target_sizes =
      topic_queue_delegate_->GetTopicSizes();
  size_t target_size_idx = 0;
  // Evenly distribute target photo sizes for the current `AmbientTheme` amongst
  // the backup photos so that the ambient UI has as much variety in photo size
  // to work with as possible.
  for (size_t i = 0; i < backup_photo_urls.size(); i++, target_size_idx++) {
    active_backup_image_downloads_.push_back(
        std::make_unique<AmbientBackupPhotoDownloader>(
            *access_token_controller_, i,
            target_sizes[target_size_idx % target_sizes.size()],
            backup_photo_urls[i],
            base::BindOnce(&AmbientPhotoController::OnBackupImageFetched,
                           weak_factory_.GetWeakPtr())));
  }
}

void AmbientPhotoController::OnBackupImageFetched(bool success) {
  if (!success) {
    // TODO(b/169807068) Change to retry individual failed images.
    active_backup_image_downloads_.clear();
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

  ambient_photo_cache::DownloadPhoto(
      topic.url, *access_token_controller_,
      base::BindOnce(&AmbientPhotoController::OnPhotoRawDataDownloaded,
                     weak_factory_.GetWeakPtr(),
                     /*is_related_image=*/false, on_done));

  if (!topic.related_image_url.empty()) {
    ::ambient::Photo* related_photo = cache_entry_.mutable_related_photo();
    related_photo->set_details(topic.related_details);
    related_photo->set_is_portrait(topic.is_portrait);
    related_photo->set_type(topic.topic_type);

    ambient_photo_cache::DownloadPhoto(
        topic.related_image_url, *access_token_controller_,
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
    ambient_photo_cache::ReadPhotoCache(
        ambient_photo_cache::Store::kBackup,
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
  ambient_photo_cache::ReadPhotoCache(
      ambient_photo_cache::Store::kPrimary, current_cache_index,
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

void AmbientPhotoController::SaveCurrentPhotoToCache() {
  // Note: WritePhotoCache could fail. The saved file name may not be
  // continuous.
  DVLOG(3) << "Save photo to cache index: " << cache_index_for_store_;
  auto current_cache_index = cache_index_for_store_;
  ++cache_index_for_store_;
  if (cache_index_for_store_ == kMaxNumberOfCachedImages) {
    cache_index_for_store_ = 0;
  }

  ambient_photo_cache::WritePhotoCache(
      ambient_photo_cache::Store::kPrimary,
      /*cache_index=*/current_cache_index, cache_entry_,
      base::BindOnce(
          [](int cache_index) {
            DVLOG(4) << "Done writing cache_index " << cache_index
                     << " to photo cache";
          },
          current_cache_index));
}
void AmbientPhotoController::DecodePhotoRawData(bool from_downloading,
                                                bool is_related_image,
                                                base::RepeatingClosure on_done,
                                                const std::string& data) {
  image_util::DecodeImageData(
      base::BindOnce(&AmbientPhotoController::OnPhotoDecoded,
                     weak_factory_.GetWeakPtr(), from_downloading,
                     is_related_image, std::move(on_done)),
      image_codec_, data);
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

  if (from_downloading) {
    SaveCurrentPhotoToCache();
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
  if (ambient_backend_model_.photo_config().IsEmpty()) {
    DVLOG(1) << "No photos should be written to model";
    // This may not be necessary because a config like this probably doesn't
    // have any photo refresh markers anyways. However, it's more technically
    // correct to be in this state instead of |kPreparingNextTopicSet|.
    state_ = State::kWaitingForNextMarker;
    return;
  }
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
