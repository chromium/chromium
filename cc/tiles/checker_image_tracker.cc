// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/checker_image_tracker.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace {
// The enum for recording checker-imaging decision UMA metric. Keep this
// consistent with the ordering in CheckerImagingDecision in enums.xml.
// Note that this enum is used to back a UMA histogram so should be treated as
// append only.
enum class CheckerImagingDecision {
  kCanChecker = 0,

  // Animation State vetoes.
  kVetoedAnimatedImage = 1,
  kVetoedVideoFrame = 2,
  // TODO(vmpstr): 3 used to be kVetoedAnimationUnknown, remove it somehow?
  kVetoedMultipartImage = 4,

  // Load state vetoes.
  kVetoedPartiallyLoadedImage = 5,
  // TODO(vmpstr): 6 used to be kVetoedLoadStateUnknown, remove it somehow?

  // Size associated vetoes.
  kVetoedSmallerThanCheckeringSize = 7,
  kVetoedLargerThanCacheSize = 8,

  // Vetoed because checkering of images has been disabled.
  kVetoedForceDisable = 9,

  // 10 used to be kVetoedNotRequiredForActivation.

  // Sync was requested by the embedder.
  kVetoedSyncRequested = 11,

  kCheckerImagingDecisionCount
};

std::string ToString(PaintImage::Id paint_image_id,
                     CheckerImagingDecision decision) {
  std::ostringstream str;
  str << "paint_image_id[" << paint_image_id << "] decision["
      << static_cast<int>(decision) << "]";
  return str.str();
}

CheckerImagingDecision GetAnimationDecision(const PaintImage& image) {
  if (image.is_multipart())
    return CheckerImagingDecision::kVetoedMultipartImage;

  switch (image.animation_type()) {
    case PaintImage::AnimationType::ANIMATED:
      return CheckerImagingDecision::kVetoedAnimatedImage;
    case PaintImage::AnimationType::VIDEO:
      return CheckerImagingDecision::kVetoedVideoFrame;
    case PaintImage::AnimationType::STATIC:
      return CheckerImagingDecision::kCanChecker;
  }

  NOTREACHED();
  return CheckerImagingDecision::kCanChecker;
}

CheckerImagingDecision GetLoadDecision(const PaintImage& image) {
  switch (image.completion_state()) {
    case PaintImage::CompletionState::DONE:
      return CheckerImagingDecision::kCanChecker;
    case PaintImage::CompletionState::PARTIALLY_DONE:
      return CheckerImagingDecision::kVetoedPartiallyLoadedImage;
  }

  NOTREACHED();
  return CheckerImagingDecision::kCanChecker;
}

CheckerImagingDecision GetSizeDecision(const SkIRect& src_rect,
                                       size_t min_bytes,
                                       size_t max_bytes) {
  // Ideally we would use the original image rect here to estimate the decode
  // duration for this image. But in the case of sprites/atlases, where small
  // subsets of this image are used across multiple tiles, re-invalidating for
  // replacing these images can incur heavy raster cost. So we use the src_rect
  // here instead.
  // TODO(khushalsagar): May be we should look at the invalidation rect for an
  // image here to detect these cases instead?
  base::CheckedNumeric<size_t> checked_size = 4;
  checked_size *= src_rect.width();
  checked_size *= src_rect.height();
  size_t size = checked_size.ValueOrDefault(std::numeric_limits<size_t>::max());

  if (size < min_bytes)
    return CheckerImagingDecision::kVetoedSmallerThanCheckeringSize;
  else if (size > max_bytes)
    return CheckerImagingDecision::kVetoedLargerThanCacheSize;
  else
    return CheckerImagingDecision::kCanChecker;
}

CheckerImagingDecision GetCheckerImagingDecision(const PaintImage& image,
                                                 const SkIRect& src_rect,
                                                 size_t min_bytes,
                                                 size_t max_bytes) {
  CheckerImagingDecision decision = GetAnimationDecision(image);
  if (decision != CheckerImagingDecision::kCanChecker)
    return decision;

  decision = GetLoadDecision(image);
  if (decision != CheckerImagingDecision::kCanChecker)
    return decision;

  return GetSizeDecision(src_rect, min_bytes, max_bytes);
}

}  // namespace

// static
const int CheckerImageTracker::kNoDecodeAllowedPriority = -1;

CheckerImageTracker::ImageDecodeRequest::ImageDecodeRequest(
    PaintImage paint_image,
    DecodeType type)
    : paint_image(std::move(paint_image)), type(type) {}

CheckerImageTracker::CheckerImageTracker(ImageController* image_controller,
                                         CheckerImageTrackerClient* client,
                                         bool enable_checker_imaging,
                                         size_t min_image_bytes_to_checker)
    : image_controller_(image_controller),
      client_(client),
      enable_checker_imaging_(enable_checker_imaging),
      min_image_bytes_to_checker_(min_image_bytes_to_checker) {}

CheckerImageTracker::~CheckerImageTracker() = default;

void CheckerImageTracker::SetNoDecodesAllowed() {
  decode_priority_allowed_ = kNoDecodeAllowedPriority;
}

void CheckerImageTracker::SetMaxDecodePriorityAllowed(DecodeType decode_type) {
  DCHECK_GT(decode_type, kNoDecodeAllowedPriority);
  DCHECK_GE(decode_type, decode_priority_allowed_);
  DCHECK_LE(decode_type, DecodeType::kLast);

  if (decode_priority_allowed_ == decode_type)
    return;
  decode_priority_allowed_ = decode_type;

  // This will start the next decode if applicable.
  ScheduleNextImageDecode();
}

void CheckerImageTracker::ScheduleImageDecodeQueue(
    ImageDecodeQueue image_decode_queue) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::ScheduleImageDecodeQueue");
#if DCHECK_IS_ON()
  // The decodes in the queue should be prioritized correctly.
  DecodeType type = DecodeType::kRaster;
  for (const auto& image_request : image_decode_queue) {
    DCHECK_GE(image_request.type, type);
    type = image_request.type;
  }
#endif

  image_decode_queue_ = std::move(image_decode_queue);
  ScheduleNextImageDecode();
}

const PaintImageIdFlatSet&
CheckerImageTracker::TakeImagesToInvalidateOnSyncTree() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::TakeImagesToInvalidateOnSyncTree");
  DCHECK_EQ(invalidated_images_on_current_sync_tree_.size(), 0u)
      << "Sync tree can not be invalidated more than once";

  invalidated_images_on_current_sync_tree_.swap(images_pending_invalidation_);
  images_pending_invalidation_.clear();
  return invalidated_images_on_current_sync_tree_;
}

void CheckerImageTracker::DidActivateSyncTree() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::DidActivateSyncTree");
  for (auto image_id : invalidated_images_on_current_sync_tree_)
    image_id_to_decode_.erase(image_id);
  invalidated_images_on_current_sync_tree_.clear();
}

void CheckerImageTracker::ClearTracker(bool can_clear_decode_policy_tracking) {
  // Unlock all images and tracking for images pending invalidation. The
  // |images_invalidated_on_current_sync_tree_| will be cleared when the sync
  // tree is activated.
  //
  // Note that we assume that any images with DecodePolicy::ASYNC, which may be
  // checkered, are safe to stop tracking here and will either be re-checkered
  // and invalidated when the decode completes or be invalidated externally.
  // This is because the policy decision for checkering an image is based on
  // inputs received from a PaintImage in the DisplayItemList. The policy chosen
  // for a PaintImage should remain unchanged.
  // If the external inputs for deciding the decode policy for an image change,
  // they should be accompanied with an invalidation during paint.
  image_id_to_decode_.clear();

  if (can_clear_decode_policy_tracking) {
    decoding_mode_map_.clear();
    image_async_decode_state_.clear();
  } else {
    // If we can't clear the decode policy, we need to make sure we still
    // re-decode and checker images that were pending invalidation.
    for (auto image_id : images_pending_invalidation_) {
      auto it = image_async_decode_state_.find(image_id);
      DCHECK(it != image_async_decode_state_.end());
      DCHECK_EQ(it->second.policy, DecodePolicy::SYNC);
      it->second.policy = DecodePolicy::ASYNC;
    }
  }
  images_pending_invalidation_.clear();
}

void CheckerImageTracker::DisallowCheckeringForImage(const PaintImage& image) {
  image_async_decode_state_.insert(
      std::make_pair(image.stable_id(), DecodeState()));
}

void CheckerImageTracker::DidFinishImageDecode(
    PaintImage::Id image_id,
    ImageController::ImageDecodeRequestId request_id,
    ImageController::ImageDecodeResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::DidFinishImageDecode");
  TRACE_EVENT_ASYNC_END0("cc", "CheckerImageTracker::DeferImageDecode",
                         image_id);

  DCHECK_NE(ImageController::ImageDecodeResult::DECODE_NOT_REQUIRED, result);
  DCHECK_EQ(outstanding_image_decode_.value().stable_id(), image_id);
  outstanding_image_decode_.reset();

  // The async decode state may have been cleared if the tracker was cleared
  // before this decode could be finished.
  auto it = image_async_decode_state_.find(image_id);
  if (it == image_async_decode_state_.end()) {
    DCHECK_EQ(image_id_to_decode_.count(image_id), 0u);
    return;
  }

  // We might have flipped this to sync while updating the hints. That function
  // would have also requested an invalidation, so we can just schedule the next
  // decode here.
  if (it->second.policy == DecodePolicy::SYNC) {
    DCHECK(decoding_mode_map_.find(image_id) != decoding_mode_map_.end());
    DCHECK_EQ(decoding_mode_map_[image_id], PaintImage::DecodingMode::kSync);

    ScheduleNextImageDecode();
    return;
  }

  it->second.policy = DecodePolicy::SYNC;
  images_pending_invalidation_.insert(image_id);
  ScheduleNextImageDecode();
  client_->NeedsInvalidationForCheckerImagedTiles();
}

bool CheckerImageTracker::ShouldCheckerImage(const DrawImage& draw_image,
                                             WhichTree tree) {
  const PaintImage& image = draw_image.paint_image();
  PaintImage::Id image_id = image.stable_id();
  TRACE_EVENT1("cc.debug", "CheckerImageTracker::ShouldCheckerImage",
               "image_id", image_id);

  // Checkering of all images is disabled.
  if (!enable_checker_imaging_)
    return false;

  // If the image was invalidated on the current sync tree and the tile is
  // for the active tree, continue checkering it on the active tree to ensure
  // the image update is atomic for the frame.
  if (invalidated_images_on_current_sync_tree_.count(image_id) != 0 &&
      tree == WhichTree::ACTIVE_TREE) {
    return true;
  }

  // If the image is pending invalidation, continue checkering it. All tiles
  // for these images will be invalidated on the next pending tree.
  if (images_pending_invalidation_.find(image_id) !=
      images_pending_invalidation_.end()) {
    return true;
  }

  auto decoding_mode_it = decoding_mode_map_.find(image_id);
  PaintImage::DecodingMode decoding_mode_hint =
      decoding_mode_it == decoding_mode_map_.end()
          ? PaintImage::DecodingMode::kUnspecified
          : decoding_mode_it->second;

  // We only checker images if the developer specifies async decoding mode.
  if (decoding_mode_hint != PaintImage::DecodingMode::kAsync)
    return false;

  auto insert_result = image_async_decode_state_.insert(
      std::pair<PaintImage::Id, DecodeState>(image_id, DecodeState()));
  auto it = insert_result.first;
  if (insert_result.second) {
    CheckerImagingDecision decision = CheckerImagingDecision::kCanChecker;
    // If the mode is sync, then don't checker this image.
    // TODO(vmpstr): Figure out if we should do something different in other
    // cases.
    if (decoding_mode_hint == PaintImage::DecodingMode::kSync)
      decision = CheckerImagingDecision::kVetoedSyncRequested;

    // The following conditions must be true for an image to be checkerable:
    //
    // 1) Complete: The data for the image should have been completely loaded.
    //
    // 2) Static: Animated images/video frames can not be checkered.
    //
    // 3) Size constraints: Small images for which the decode is expected to
    // be fast and large images which would breach the image cache budget and
    // go through the at-raster decode path are not checkered.
    //
    // 4) Multipart images: Multipart images can be used to display mjpg video
    // frames, checkering which would cause each video frame to flash and
    // therefore should not be checkered.
    //
    // Note that we only need to do this check if we didn't veto above in this
    // block.
    if (decision == CheckerImagingDecision::kCanChecker) {
      decision = GetCheckerImagingDecision(
          image, draw_image.src_rect(), min_image_bytes_to_checker_,
          image_controller_->image_cache_max_limit_bytes());
    }

    if (decision == CheckerImagingDecision::kCanChecker && force_disabled_) {
      // Get the decision for all the veto reasons first, so we can UMA the
      // images that were not checkered only because checker-imaging was force
      // disabled.
      decision = CheckerImagingDecision::kVetoedForceDisable;
    }

    it->second.policy = decision == CheckerImagingDecision::kCanChecker
                            ? DecodePolicy::ASYNC
                            : DecodePolicy::SYNC;

    UMA_HISTOGRAM_ENUMERATION(
        "Compositing.Renderer.CheckerImagingDecision", decision,
        CheckerImagingDecision::kCheckerImagingDecisionCount);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "CheckerImageTracker::CheckerImagingDecision", "image_params",
                 ToString(image_id, decision));
  }

  // Update the decode state from the latest image we have seen. Note that it
  // is not necessary to perform this in the early out cases above since in
  // each of those cases the image has already been decoded.
  UpdateDecodeState(draw_image, image_id, &it->second);

  return it->second.policy == DecodePolicy::ASYNC;
}

void CheckerImageTracker::UpdateDecodeState(const DrawImage& draw_image,
                                            PaintImage::Id paint_image_id,
                                            DecodeState* decode_state) {
  // If the policy is not async then either we decoded this image already or
  // we decided not to ever checker it.
  if (decode_state->policy != DecodePolicy::ASYNC)
    return;

  // If the decode is already in flight, then we will have to live with what we
  // have now.
  if (outstanding_image_decode_.has_value() &&
      outstanding_image_decode_.value().stable_id() == paint_image_id) {
    return;
  }

  // Choose the max scale and filter quality. This keeps the memory usage to the
  // minimum possible while still increasing the possibility of getting a cache
  // hit.
  decode_state->scale = SkSize::Make(
      std::max(decode_state->scale.fWidth, draw_image.scale().fWidth),
      std::max(decode_state->scale.fHeight, draw_image.scale().fHeight));
  decode_state->filter_quality =
      std::max(decode_state->filter_quality, draw_image.filter_quality());
  decode_state->color_space = draw_image.target_color_space();
  decode_state->frame_index = draw_image.frame_index();
}

void CheckerImageTracker::ScheduleNextImageDecode() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::ScheduleNextImageDecode");
  // We can have only one outstanding decode pending completion with the decode
  // service. We'll come back here when it is completed.
  if (outstanding_image_decode_.has_value())
    return;

  if (image_decode_queue_.empty())
    return;

  // If scheduling decodes for this priority is not allowed right now, don't
  // schedule them. We will come back here when the allowed priority changes.
  if (image_decode_queue_.front().type > decode_priority_allowed_)
    return;

  DrawImage draw_image;
  while (!image_decode_queue_.empty()) {
    auto candidate = std::move(image_decode_queue_.front().paint_image);
    image_decode_queue_.erase(image_decode_queue_.begin());

    // Once an image has been decoded, it can still be present in the decode
    // queue (duplicate entries), or while an image is still being skipped on
    // the active tree. Check if the image is still ASYNC to see if a decode is
    // needed.
    PaintImage::Id image_id = candidate.stable_id();
    auto it = image_async_decode_state_.find(image_id);
    DCHECK(it != image_async_decode_state_.end());
    if (it->second.policy != DecodePolicy::ASYNC)
      continue;

    draw_image = DrawImage(
        candidate, SkIRect::MakeWH(candidate.width(), candidate.height()),
        it->second.filter_quality,
        SkMatrix::MakeScale(it->second.scale.width(),
                            it->second.scale.height()),
        it->second.frame_index, it->second.color_space);
    outstanding_image_decode_.emplace(candidate);
    break;
  }

  // We either found an image to decode or we reached the end of the queue. If
  // we couldn't find an image, we're done.
  if (!outstanding_image_decode_.has_value()) {
    DCHECK(image_decode_queue_.empty());
    return;
  }

  PaintImage::Id image_id = outstanding_image_decode_.value().stable_id();
  DCHECK_EQ(image_id_to_decode_.count(image_id), 0u);
  TRACE_EVENT_ASYNC_BEGIN0("cc", "CheckerImageTracker::DeferImageDecode",
                           image_id);
  ImageController::ImageDecodeRequestId request_id =
      image_controller_->QueueImageDecode(
          draw_image, base::BindOnce(&CheckerImageTracker::DidFinishImageDecode,
                                     weak_factory_.GetWeakPtr(), image_id));

  image_id_to_decode_.emplace(image_id, std::make_unique<ScopedDecodeHolder>(
                                            image_controller_, request_id));
}

void CheckerImageTracker::UpdateImageDecodingHints(
    base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
        decoding_mode_map) {
  // Merge the |decoding_mode_map| with our member map, keeping the more
  // conservative values.
  // TODO(vmpstr): Figure out if and how do we clear this value to ensure that
  // if we no longer have any kSync images, for example, then we can loosen the
  // requirement on the decoding mode for that image id.
  for (auto pair : decoding_mode_map) {
    PaintImage::Id id = pair.first;
    PaintImage::DecodingMode decoding_mode = pair.second;

    // In case we already have this image as async, it implies that we are
    // currently displaying this content as checkered. We can flip the state to
    // sync here and add the image to be invalidated. The invalidation should
    // happen shortly after, since this function should be called in a commit.
    auto state_it = image_async_decode_state_.find(id);
    if (state_it != image_async_decode_state_.end()) {
      auto& state = state_it->second;
      if (state.policy == DecodePolicy::ASYNC &&
          decoding_mode == PaintImage::DecodingMode::kSync) {
        state.policy = DecodePolicy::SYNC;
        images_pending_invalidation_.insert(id);
      }
    }

    // Update the decoding hints map.
    auto decoding_mode_it = decoding_mode_map_.find(id);
    if (decoding_mode_it == decoding_mode_map_.end()) {
      decoding_mode_map_[id] = decoding_mode;
    } else {
      decoding_mode_it->second =
          PaintImage::GetConservative(decoding_mode_it->second, decoding_mode);
    }
  }
}

}  // namespace cc
