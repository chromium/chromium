// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/image_animation_controller.h"

#include <algorithm>
#include <sstream>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "cc/paint/image_animation_count.h"

namespace cc {
namespace {

// The maximum number of time an animation can be delayed before it is reset to
// start from the beginning, instead of fast-forwarding to catch up to the
// desired frame.
const base::TimeDelta kAnimationResyncCutoff = base::Minutes(5);

// Given the |desired_frame_time|, returns the time of the tick it should be
// snapped to.
base::TimeTicks SnappedTickTimeFromFrameTime(
    const viz::BeginFrameArgs& args,
    base::TimeTicks desired_frame_time) {
  auto snapped_tick_time =
      desired_frame_time.SnappedToNextTick(args.frame_time, args.interval);
  DCHECK_GE(snapped_tick_time, desired_frame_time);

  if (snapped_tick_time != desired_frame_time)
    snapped_tick_time -= args.interval;
  return snapped_tick_time;
}

}  //  namespace

ImageAnimationController::ImageAnimationController(
    base::SingleThreadTaskRunner* task_runner,
    Client* client,
    bool enable_image_animation_resync)
    : scheduler_(task_runner, client),
      enable_image_animation_resync_(enable_image_animation_resync),
      use_resume_behavior_(
          base::FeatureList::IsEnabled(features::kAnimatedImageResume)) {}

ImageAnimationController::~ImageAnimationController() = default;

void ImageAnimationController::UpdateAnimatedImage(
    const DiscardableImageMap::AnimatedImageMetadata& data) {
  AnimationState& animation_state = animation_state_map_[data.paint_image_id];
  animation_state.UpdateMetadata(data);
}

void ImageAnimationController::RegisterAnimationDriver(
    PaintImage::Id paint_image_id,
    AnimationDriver* driver) {
  auto it = animation_state_map_.find(paint_image_id);
  CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
  it->second.AddDriver(driver);
  registered_animations_.insert(paint_image_id);
}

void ImageAnimationController::UnregisterAnimationDriver(
    PaintImage::Id paint_image_id,
    AnimationDriver* driver) {
  auto it = animation_state_map_.find(paint_image_id);
  CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
  it->second.RemoveDriver(driver);
  if (!it->second.has_drivers())
    registered_animations_.erase(paint_image_id);
}

bool ImageAnimationController::IsRegistered(PaintImage::Id paint_image_id) {
  return animation_state_map_.contains(paint_image_id);
}

const PaintImageIdFlatSet& ImageAnimationController::AnimateForSyncTree(
    const viz::BeginFrameArgs& args) {
  TRACE_EVENT1("cc", "ImageAnimationController::AnimateImagesForSyncTree",
               "frame_time_from_now",
               (base::TimeTicks::Now() - args.frame_time).InMillisecondsF());
  DCHECK(images_animated_on_sync_tree_.empty());

  scheduler_.WillAnimate();
  std::optional<base::TimeTicks> next_invalidation_time;

  for (auto id : registered_animations_) {
    auto it = animation_state_map_.find(id);
    CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
    AnimationState& state = it->second;

    // Is anyone still interested in animating this image?
    state.UpdateStateFromDrivers();
    if (!state.ShouldAnimate()) {
      TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                           "ShouldAnimate - early out",
                           TRACE_EVENT_SCOPE_THREAD);
      continue;
    }

    // If we were able to advance this animation, invalidate it on the sync
    // tree.
    if (state.AdvanceFrame(args, enable_image_animation_resync_,
                           use_resume_behavior_))
      images_animated_on_sync_tree_.insert(id);

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "AnimationState", TRACE_EVENT_SCOPE_THREAD, "state",
                         state.ToString());
    // Update the next invalidation time to the earliest time at which we need
    // a frame to animate an image.
    // Note its important to check ShouldAnimate() here again since advancing to
    // a new frame on the sync tree means we might not need to animate this
    // image any longer.
    if (!state.ShouldAnimate())
      continue;

    DCHECK_GT(state.next_desired_tick_time(), args.frame_time);
    if (!next_invalidation_time.has_value()) {
      next_invalidation_time.emplace(state.next_desired_tick_time());
    } else {
      next_invalidation_time = std::min(state.next_desired_tick_time(),
                                        next_invalidation_time.value());
    }
  }

  if (next_invalidation_time.has_value())
    scheduler_.Schedule(next_invalidation_time.value());
  else
    scheduler_.Cancel();

  return images_animated_on_sync_tree_;
}

void ImageAnimationController::UpdateStateFromDrivers() {
  TRACE_EVENT0("cc", "UpdateStateFromAnimationDrivers");

  std::optional<base::TimeTicks> next_invalidation_time;
  for (auto image_id : registered_animations_) {
    auto it = animation_state_map_.find(image_id);
    CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
    AnimationState& state = it->second;
    state.UpdateStateFromDrivers();

    // Note that by not updating the |next_invalidation_time| from this image
    // here, we will cancel any pending invalidation scheduled for this image
    // when updating the |scheduler_| at the end of this loop.
    if (!state.ShouldAnimate())
      continue;

    if (!next_invalidation_time.has_value()) {
      next_invalidation_time.emplace(state.next_desired_tick_time());
    } else {
      next_invalidation_time = std::min(next_invalidation_time.value(),
                                        state.next_desired_tick_time());
    }
  }

  if (next_invalidation_time.has_value())
    scheduler_.Schedule(next_invalidation_time.value());
  else
    scheduler_.Cancel();
}

void ImageAnimationController::DidActivate() {
  TRACE_EVENT0("cc", "ImageAnimationController::WillActivate");

  for (auto id : images_animated_on_sync_tree_) {
    auto it = animation_state_map_.find(id);
    CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
    it->second.PushPendingToActive();
  }
  images_animated_on_sync_tree_.clear();

  // We would retain state for images with no drivers (no recordings) to allow
  // resuming of animations. However, since the animation will be re-started
  // from the beginning after navigation, we can avoid maintaining the state.
  if (did_navigate_) {
    for (auto it = animation_state_map_.begin();
         it != animation_state_map_.end();) {
      if (it->second.has_drivers())
        it++;
      else
        it = animation_state_map_.erase(it);
    }
    did_navigate_ = false;
  }
}

size_t ImageAnimationController::GetFrameIndexForImage(
    PaintImage::Id paint_image_id,
    WhichTree tree) const {
  const auto& it = animation_state_map_.find(paint_image_id);
  CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
  return tree == WhichTree::PENDING_TREE ? it->second.pending_index()
                                         : it->second.active_index();
}

void ImageAnimationController::WillBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  scheduler_.WillBeginImplFrame(args);
}

const base::flat_set<
    raw_ptr<ImageAnimationController::AnimationDriver, CtnExperimental>>&
ImageAnimationController::GetDriversForTesting(
    PaintImage::Id paint_image_id) const {
  const auto& it = animation_state_map_.find(paint_image_id);
  CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
  return it->second.drivers_for_testing();
}

size_t ImageAnimationController::GetLastNumOfFramesSkippedForTesting(
    PaintImage::Id paint_image_id) const {
  const auto& it = animation_state_map_.find(paint_image_id);
  CHECK(it != animation_state_map_.end(), base::NotFatalUntil::M130);
  return it->second.last_num_frames_skipped_for_testing();
}

std::optional<ImageAnimationController::ConsistentFrameDuration>
ImageAnimationController::GetConsistentContentFrameDuration() {
  if (animation_state_map_.empty()) {
    return std::nullopt;
  }
  std::optional<base::TimeDelta> frame_duration;
  uint32_t num_images = 0u;
  for (auto& [id, state] : animation_state_map_) {
    if (!state.ShouldAnimate()) {
      continue;
    }
    std::optional<base::TimeDelta> image_frame_duration =
        state.GetConsistentContentFrameDuration();
    if (!image_frame_duration) {
      return std::nullopt;
    }
    if (frame_duration &&
        frame_duration.value() != image_frame_duration.value()) {
      return std::nullopt;
    }
    frame_duration = image_frame_duration.value();
    num_images++;
  }
  if (!frame_duration) {
    return std::nullopt;
  }
  return ConsistentFrameDuration{frame_duration.value(), num_images};
}

ImageAnimationController::AnimationState::AnimationState() = default;

ImageAnimationController::AnimationState::AnimationState(
    AnimationState&& other) = default;

ImageAnimationController::AnimationState&
ImageAnimationController::AnimationState::operator=(AnimationState&& other) =
    default;

ImageAnimationController::AnimationState::~AnimationState() {
  DCHECK(drivers_.empty());
}

bool ImageAnimationController::AnimationState::ShouldAnimate() const {
  return ShouldAnimate(current_state_.repetitions_completed,
                       current_state_.pending_index);
}

bool ImageAnimationController::AnimationState::ShouldAnimate(
    int repetitions_completed,
    size_t pending_index) const {
  DCHECK(current_state_.repetitions_completed == 0 || is_complete());

  // If we have no drivers for this image, no need to animate it.
  if (!should_animate_from_drivers_)
    return false;

  switch (requested_repetitions_) {
    case kAnimationLoopOnce:
      if (repetitions_completed >= 1)
        return false;
      break;
    case kAnimationNone:
      NOTREACHED() << "We shouldn't be tracking kAnimationNone images";
    case kAnimationLoopInfinite:
      break;
    default:
      if (requested_repetitions_ <= repetitions_completed)
        return false;
  }

  // If we have not yet received all data for this image, we can not advance to
  // an incomplete frame.
  if (!frames_[NextFrameIndex(pending_index)].complete)
    return false;

  // If we don't have all data for this image, we can not trust the frame count
  // and loop back to the first frame.
  size_t last_frame_index = frames_.size() - 1;
  if (completion_state_ != PaintImage::CompletionState::kDone &&
      pending_index == last_frame_index) {
    return false;
  }

  return true;
}

// SCHEDULING
// The rate at which the animation progresses is decided by the frame duration,
// the time for which a particular frame should be displayed, specified in the
// metadata for the image. The animation is assumed to start from the frame_time
// of the first BeginFrame after the animation is registered and is visible.
// From here on, the time at which a frame should be displayed is the sum of
// durations for all previous frames of the animation.
// But, in order to align the work for the animation update with an impl frame,
// invalidations are requested at the beginning of the frame boundary in which
// the frame should be displayed.
bool ImageAnimationController::AnimationState::AdvanceFrame(
    const viz::BeginFrameArgs& args,
    bool enable_image_animation_resync,
    bool use_resume_behavior) {
  DCHECK(ShouldAnimate(current_state_.repetitions_completed,
                       current_state_.pending_index));
  const base::TimeTicks next_tick_time = args.frame_time + args.interval;

  // Start the animation from the first frame, if not yet started. The code
  // falls through to catching up if the duration for the first frame is less
  // than the interval.
  if (!animation_started_) {
    DCHECK_EQ(current_state_.pending_index, 0u);

    animation_started_time_ = args.frame_time;
    current_state_.next_desired_frame_time =
        args.frame_time + frames_[0].duration;
    current_state_.next_desired_tick_time = SnappedTickTimeFromFrameTime(
        args, current_state_.next_desired_frame_time);
    animation_started_ = true;
  }

  // Don't advance the animation if its not time yet to move to the next frame.
  if (args.frame_time < current_state_.next_desired_tick_time)
    return needs_invalidation();

  // If the animation is more than 5 min out of date, we don't bother catching
  // up and start again from the current frame.
  // Note that we don't need to invalidate this image since the active tree
  // is already displaying the current frame.
  if (enable_image_animation_resync &&
      args.frame_time - current_state_.next_desired_frame_time >
          kAnimationResyncCutoff) {
    TRACE_EVENT_INSTANT0("cc", "Resync - early out", TRACE_EVENT_SCOPE_THREAD);
    DCHECK_EQ(current_state_.pending_index, active_index_);
    current_state_.next_desired_frame_time =
        args.frame_time + frames_[current_state_.pending_index].duration;
    current_state_.next_desired_tick_time =
        std::max(SnappedTickTimeFromFrameTime(
                     args, current_state_.next_desired_frame_time),
                 next_tick_time);
    return needs_invalidation();
  }

  current_state_.num_of_frames_advanced = 0u;
  if (use_resume_behavior) {
    // When using the resume method, run the animation advancement starting
    // at the current frame time rather than the saved tick time.

    // Advance only as many frames as would fit in the display rate.
    // IE if the display refresh rate is 60 Hz and the animated image updated
    // every 11 ms, we could have a display frame that spans 2 animation
    // frames.
    current_state_ = AdvanceAnimationState(
        current_state_, args, args.frame_time, enable_image_animation_resync);
  } else {
    // Keep catching up the animation from the last saved tick time until we
    // reach the frame we should be displaying now.
    current_state_ = AdvanceAnimationState(
        current_state_, args, current_state_.next_desired_tick_time,
        enable_image_animation_resync);
  }
  DCHECK_GE(current_state_.num_of_frames_advanced, 1u);
  last_num_frames_skipped_ = current_state_.num_of_frames_advanced - 1u;

  return needs_invalidation();
}

ImageAnimationController::AnimationState::AnimationAdvancementState
ImageAnimationController::AnimationState::AdvanceAnimationState(
    AnimationAdvancementState animation_advancement_state,
    const viz::BeginFrameArgs& args,
    base::TimeTicks start,
    bool enable_image_animation_resync) const {
  const base::TimeTicks end = args.frame_time + args.interval;
  base::TimeTicks elapsed_time = start;
  while (elapsed_time < end &&
         ShouldAnimate(animation_advancement_state.repetitions_completed,
                       animation_advancement_state.pending_index)) {
    animation_advancement_state.num_of_frames_advanced++;
    size_t next_frame_index =
        NextFrameIndex(animation_advancement_state.pending_index);
    elapsed_time += frames_[next_frame_index].duration;
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "FrameDurationIndex", TRACE_EVENT_SCOPE_THREAD,
                         "frame_index", next_frame_index, "duration",
                         frames_[next_frame_index].duration.InMillisecondsF());
    base::TimeTicks next_desired_frame_time =
        animation_advancement_state.next_desired_frame_time +
        frames_[next_frame_index].duration;

    // The image may load more slowly than it's supposed to animate, so that by
    // the time we reach the end of the first repetition, we're well behind.
    // Start the animation from the first frame in this case, so that we don't
    // skip frames (or whole iterations) trying to "catch up". This is a
    // tradeoff: It guarantees users see the whole animation the second time
    // through and don't miss any repetitions, and is closer to what other
    // browsers do; on the other hand, it makes animations "less accurate" for
    // pages that try to sync an image and some other resource (e.g. audio),
    // especially if users switch tabs (and thus stop drawing the animation,
    // which will pause it) during that initial loop, then switch back later.
    if (enable_image_animation_resync && next_frame_index == 0u &&
        animation_advancement_state.repetitions_completed == 1 &&
        next_desired_frame_time <= args.frame_time) {
      animation_advancement_state.pending_index = 0u;
      animation_advancement_state.next_desired_frame_time =
          args.frame_time + frames_[0].duration;
      animation_advancement_state.next_desired_tick_time = std::max(
          SnappedTickTimeFromFrameTime(
              args, animation_advancement_state.next_desired_frame_time),
          end);
      animation_advancement_state.repetitions_completed = 0;
      break;
    }

    animation_advancement_state.pending_index = next_frame_index;
    animation_advancement_state.next_desired_frame_time =
        next_desired_frame_time;
    animation_advancement_state.next_desired_tick_time =
        SnappedTickTimeFromFrameTime(
            args, animation_advancement_state.next_desired_frame_time);

    // If we are advancing to the last frame and the image has been completely
    // loaded (which means that the frame count is known to be accurate), we
    // just finished a loop in the animation.
    if (animation_advancement_state.pending_index == frames_.size() - 1 &&
        is_complete())
      animation_advancement_state.repetitions_completed++;
  }

  // We should have advanced a single frame, anything more than that are frames
  // skipped trying to catch up.
  DCHECK_GT(animation_advancement_state.num_of_frames_advanced, 0u);
  size_t frames_skipped =
      animation_advancement_state.num_of_frames_advanced - 1u;
  switch (animation_advancement_state.repetitions_completed) {
    case 0:
      UMA_HISTOGRAM_COUNTS_100000(
          "AnimatedImage.NumOfFramesSkipped.FirstAnimationLoop",
          frames_skipped);
      break;
    case 1:
      UMA_HISTOGRAM_COUNTS_100000(
          "AnimatedImage.NumOfFramesSkipped.SecondAnimationLoop",
          frames_skipped);
      break;
    case 2:
    case 3:
    case 4:
      UMA_HISTOGRAM_COUNTS_100000(
          "AnimatedImage.NumOfFramesSkipped.ThirdToFifthAnimationLoop",
          frames_skipped);
      break;
  }
  UMA_HISTOGRAM_COUNTS_100000("AnimatedImage.NumOfFramesSkipped.Compositor",
                              frames_skipped);

  return animation_advancement_state;
}

void ImageAnimationController::AnimationState::UpdateMetadata(
    const DiscardableImageMap::AnimatedImageMetadata& data) {
  paint_image_id_ = data.paint_image_id;

  DCHECK_NE(data.repetition_count, kAnimationNone);
  requested_repetitions_ = data.repetition_count;

  DCHECK(frames_.size() <= data.frames.size())
      << "Updated recordings can only append frames";
  frames_ = data.frames;
  cached_consistent_frame_duration_valid_ = false;
  DCHECK_GT(frames_.size(), 1u);

  DCHECK(completion_state_ != PaintImage::CompletionState::kDone ||
         data.completion_state == PaintImage::CompletionState::kDone)
      << "If the image was marked complete before, it can not be incomplete in "
         "a new update";
  completion_state_ = data.completion_state;

  // Update the repetition count in case we have displayed the last frame and
  // we now know the frame count to be accurate.
  size_t last_frame_index = frames_.size() - 1;
  if (current_state_.pending_index == last_frame_index && is_complete() &&
      current_state_.repetitions_completed == 0)
    current_state_.repetitions_completed++;

  // Reset the animation if the sequence id received in this recording was
  // incremented.
  if (reset_animation_sequence_id_ < data.reset_animation_sequence_id) {
    reset_animation_sequence_id_ = data.reset_animation_sequence_id;
    ResetAnimation();
  }
}

void ImageAnimationController::AnimationState::PushPendingToActive() {
  active_index_ = current_state_.pending_index;
}

std::optional<base::TimeDelta>
ImageAnimationController::AnimationState::GetConsistentContentFrameDuration() {
  if (!cached_consistent_frame_duration_valid_) {
    ComputeConsistentContentFrameDuration();
  }
  cached_consistent_frame_duration_valid_ = true;
  if (!cached_has_consistent_frame_duration_) {
    return std::nullopt;
  }
  return cached_consistent_frame_duration_;
}

void ImageAnimationController::AnimationState::
    ComputeConsistentContentFrameDuration() {
  cached_has_consistent_frame_duration_ = false;
  std::optional<base::TimeDelta> frame_duration;
  for (const auto& metadata : frames_) {
    if (frame_duration && frame_duration.value() != metadata.duration) {
      return;
    }
    frame_duration = metadata.duration;
  }
  if (frame_duration) {
    cached_has_consistent_frame_duration_ = true;
    cached_consistent_frame_duration_ = frame_duration.value();
  }
}

void ImageAnimationController::AnimationState::AddDriver(
    AnimationDriver* driver) {
  drivers_.insert(driver);
}

void ImageAnimationController::AnimationState::RemoveDriver(
    AnimationDriver* driver) {
  drivers_.erase(driver);
}

void ImageAnimationController::AnimationState::UpdateStateFromDrivers() {
  should_animate_from_drivers_ = false;
  for (AnimationDriver* driver : drivers_) {
    if (driver->ShouldAnimate(paint_image_id_)) {
      should_animate_from_drivers_ = true;
      break;
    }
  }
}

void ImageAnimationController::AnimationState::ResetAnimation() {
  animation_started_ = false;
  current_state_.next_desired_frame_time = base::TimeTicks();
  current_state_.repetitions_completed = 0;
  current_state_.pending_index = 0u;
  // Don't reset the |active_index_|, tiles on the active tree still need it.
}

std::string ImageAnimationController::AnimationState::ToString() const {
  std::ostringstream str;
  str << "paint_image_id[" << paint_image_id_ << "]\nrequested_repetitions["
      << requested_repetitions_ << "]\nrepetitions_completed["
      << requested_repetitions_ << "]\ndrivers[" << drivers_.size()
      << "]\nactive_index[" << active_index_ << "]\npending_index["
      << current_state_.pending_index << "]\nnext_desired_frame_time["
      << (current_state_.next_desired_frame_time - animation_started_time_)
             .InMillisecondsF()
      << "]\nnext_desired_tick_time["
      << (current_state_.next_desired_tick_time - animation_started_time_)
             .InMillisecondsF()
      << "]\nshould_animate_from_drivers[" << should_animate_from_drivers_
      << "]\ncompletion_state[" << static_cast<int>(completion_state_) << "]";
  return str.str();
}

size_t ImageAnimationController::AnimationState::NextFrameIndex(
    size_t pending_index) const {
  if (!animation_started_)
    return 0u;
  return (pending_index + 1) % frames_.size();
}

ImageAnimationController::InvalidationScheduler::InvalidationScheduler(
    base::SingleThreadTaskRunner* task_runner,
    Client* client)
    : task_runner_(task_runner), client_(client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

ImageAnimationController::InvalidationScheduler::~InvalidationScheduler() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void ImageAnimationController::InvalidationScheduler::Schedule(
    base::TimeTicks animation_time) {
  auto now = now_callback_for_testing_.is_null()
                 ? base::TimeTicks::Now()
                 : now_callback_for_testing_.Run();

  // If an animation or impl frame is already pending, don't schedule another
  // notification. We will schedule the next notification based on the latest
  // animation state during AnimateForSyncTree, or in WillBeginImplFrame if
  // needed.
  if (state_ == InvalidationState::kPendingInvalidation ||
      state_ == InvalidationState::kPendingImplFrame) {
    return;
  }

  // The requested notification time can be in the past. For instance, if an
  // animation was paused because the image became invisible.
  if (animation_time < now)
    animation_time = now;

  // If we already have a notification scheduled to run at this time, no need to
  // Cancel it.
  if (state_ == InvalidationState::kPendingRequestBeginFrame &&
      animation_time == next_animation_time_)
    return;

  // Cancel the pending notification since we the requested notification time
  // has changed.
  Cancel();

  base::TimeDelta delta = animation_time - now;
  TRACE_EVENT1("cc", "ScheduleFrameForImageAnimation", "delta",
               delta.InMillisecondsF());

  state_ = InvalidationState::kPendingRequestBeginFrame;
  next_animation_time_ = animation_time;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InvalidationScheduler::RequestBeginFrame,
                     weak_factory_.GetWeakPtr()),
      delta);
}

void ImageAnimationController::InvalidationScheduler::Cancel() {
  state_ = InvalidationState::kIdle;
  weak_factory_.InvalidateWeakPtrs();
}

void ImageAnimationController::InvalidationScheduler::RequestBeginFrame() {
  TRACE_EVENT0(
      "cc",
      "ImageAnimationController::InvalidationScheduler::RequestBeginFrame");
  DCHECK_EQ(state_, InvalidationState::kPendingRequestBeginFrame);

  state_ = InvalidationState::kPendingImplFrame;
  client_->RequestBeginFrameForAnimatedImages();
}

void ImageAnimationController::InvalidationScheduler::WillAnimate() {
  // Unless the sync tree came after we explicitly requested an invalidation,
  // we can't proceed to idle state.
  if (state_ != InvalidationState::kPendingInvalidation)
    return;

  state_ = InvalidationState::kIdle;
  next_animation_time_ = base::TimeTicks();
}

void ImageAnimationController::InvalidationScheduler::WillBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  switch (state_) {
    case InvalidationState::kIdle:
    case InvalidationState::kPendingInvalidation:
      // We don't need an invalidation or one is already pending. In either
      // case, no state change is needed.
      break;
    case InvalidationState::kPendingRequestBeginFrame:
      if (args.frame_time >= next_animation_time_) {
        // If we can make progress on the animation in this impl frame request
        // an invalidation.
        RequestInvalidation();
      }
      // Otherwise the pending notification will ensure we get another impl
      // impl frame at the |next_animation_time_|.
      break;
    case InvalidationState::kPendingImplFrame:
      if (args.frame_time >= next_animation_time_) {
        // We received the impl frame needed for the animation, request an
        // invalidation.
        RequestInvalidation();
      } else {
        // Ideally this should be the impl frame we wanted, so we should always
        // be able to animate at this frame. But that might not be the case if
        // we get a missed BeginFrame. In that case, make a request for the next
        // impl frame.
        client_->RequestBeginFrameForAnimatedImages();
      }
      break;
  }
}

void ImageAnimationController::InvalidationScheduler::RequestInvalidation() {
  TRACE_EVENT0(
      "cc",
      "ImageAnimationController::InvalidationScheduler::RequestInvalidation");
  DCHECK_NE(state_, InvalidationState::kIdle);
  DCHECK_NE(state_, InvalidationState::kPendingInvalidation);

  // Any pending notification can/should be cancelled once an invalidation is
  // requested because each time we animate a sync tree, we schedule a task for
  // the next animation update if necessary.
  Cancel();

  state_ = InvalidationState::kPendingInvalidation;
  client_->RequestInvalidationForAnimatedImages();
}

}  // namespace cc
