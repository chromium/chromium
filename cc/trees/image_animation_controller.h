// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_IMAGE_ANIMATION_CONTROLLER_H_
#define CC_TREES_IMAGE_ANIMATION_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class PaintImage;

// ImageAnimationController is responsible for tracking state for ticking image
// animations in the compositor.
//
// 1) It receives the updated metadata for these images from new recordings
//    received from the client using UpdateAnimatedImage. The controller tracks
//    the frame index of an image used on a tree, and advances the animation to
//    the desired frame each time a new sync tree is created.
//
// 2) An AnimationDriver can register itself for deciding whether the
//    controller animates an image. The animation is paused if there are no
//    registered drivers interested in animating it.
//
//  3) An animation is only advanced on the sync tree, which is requested to be
//     created using the |client| callbacks. This effectively means that
//     the frame of the image used remains consistent throughout the lifetime of
//     a tree, guaranteeing that the image update is atomic.
class CC_EXPORT ImageAnimationController {
 public:
  // AnimationDrivers are clients interested in driving image animations. An
  // animation is ticked if there is at least one driver registered for which
  // ShouldAnimate returns true. Once
  // no drivers are registered for an image, or none of the registered drivers
  // want us to animate, the animation is no longer ticked.
  class CC_EXPORT AnimationDriver {
   public:
    virtual ~AnimationDriver() {}

    // Returns true if the image should be animated.
    virtual bool ShouldAnimate(PaintImage::Id paint_image_id) const = 0;
  };

  class CC_EXPORT Client {
   public:
    virtual ~Client() {}

    // Notifies the client that an impl frame is needed to animate an image.
    virtual void RequestBeginFrameForAnimatedImages() = 0;

    // Notifies the client that a sync tree is needed to invalidate the animated
    // images in this impl frame. This should only be called from within an impl
    // frame.
    virtual void RequestInvalidationForAnimatedImages() = 0;
  };

  // |task_runner| is the thread on which the controller is used. The |client|
  // can only be called on this thread.
  // |enable_image_animation_resync| specifies whether the animation can be
  // reset to the beginning to avoid skipping many frames.
  ImageAnimationController(base::SingleThreadTaskRunner* task_runner,
                           Client* client,
                           bool enable_image_animation_resync);
  ~ImageAnimationController();

  // Called to update the state for a PaintImage received in a new recording.
  void UpdateAnimatedImage(
      const DiscardableImageMap::AnimatedImageMetadata& data);

  // Registers/Unregisters an animation driver interested in animating this
  // image.
  // Note that the state for this image must have been populated to the
  // controller using UpdatePaintImage prior to registering any drivers.
  void RegisterAnimationDriver(PaintImage::Id paint_image_id,
                               AnimationDriver* driver);
  void UnregisterAnimationDriver(PaintImage::Id paint_image_id,
                                 AnimationDriver* driver);
  bool IsRegistered(PaintImage::Id paint_image_id);

  // Called to advance the animations to the frame to be used on the sync tree.
  // This should be called only once for a sync tree and must be followed with
  // a call to DidActivate when this tree is activated.
  // Returns the set of images that were animated and should be invalidated on
  // this sync tree.
  const PaintImageIdFlatSet& AnimateForSyncTree(
      const viz::BeginFrameArgs& args);

  // Called whenever the ShouldAnimate response for a driver could have changed.
  // For instance on a change in the visibility of the image, we would pause
  // off-screen animations.
  // This is called after every DrawProperties update and commit.
  void UpdateStateFromDrivers();

  // Called when the sync tree was activated and the animations' associated
  // state should be pushed to the active tree.
  void DidActivate();

  // Returns the frame index to use for the given PaintImage and tree.
  size_t GetFrameIndexForImage(PaintImage::Id paint_image_id,
                               WhichTree tree) const;

  // Notifies the beginning of an impl frame with the given |args|.
  void WillBeginImplFrame(const viz::BeginFrameArgs& args);

  bool did_navigate() const { return did_navigate_; }
  void set_did_navigate() { did_navigate_ = true; }

  const base::flat_set<raw_ptr<AnimationDriver, CtnExperimental>>&
  GetDriversForTesting(PaintImage::Id paint_image_id) const;
  size_t GetLastNumOfFramesSkippedForTesting(
      PaintImage::Id paint_image_id) const;

  size_t animation_state_map_size_for_testing() {
    return animation_state_map_.size();
  }

  using NowCallback = base::RepeatingCallback<base::TimeTicks()>;
  void set_now_callback_for_testing(NowCallback cb) {
    scheduler_.set_now_callback_for_testing(cb);
  }

  // If all animating images have the same frame duration, then returns the
  // frame duration and number of images.
  struct ConsistentFrameDuration {
    base::TimeDelta frame_duration;
    uint32_t num_images;
  };
  std::optional<ConsistentFrameDuration> GetConsistentContentFrameDuration();

 private:
  class AnimationState {
   public:
    AnimationState();
    AnimationState(const AnimationState&) = delete;
    AnimationState(AnimationState&& other);
    ~AnimationState();

    AnimationState& operator=(const AnimationState&) = delete;
    AnimationState& operator=(AnimationState&& other);

    bool ShouldAnimate() const;
    bool ShouldAnimate(int repetitions_completed, size_t pending_index) const;
    bool AdvanceFrame(const viz::BeginFrameArgs& args,
                      bool enable_image_animation_resync,
                      bool use_resume_behavior);
    void UpdateMetadata(const DiscardableImageMap::AnimatedImageMetadata& data);
    void PushPendingToActive();
    // If all frames have same frame duration, return that duration.
    std::optional<base::TimeDelta> GetConsistentContentFrameDuration();

    void AddDriver(AnimationDriver* driver);
    void RemoveDriver(AnimationDriver* driver);
    void UpdateStateFromDrivers();
    bool has_drivers() const { return !drivers_.empty(); }

    size_t pending_index() const { return current_state_.pending_index; }
    size_t active_index() const { return active_index_; }
    base::TimeTicks next_desired_tick_time() const {
      return current_state_.next_desired_tick_time;
    }
    const base::flat_set<raw_ptr<AnimationDriver, CtnExperimental>>&
    drivers_for_testing() const {
      return drivers_;
    }
    size_t last_num_frames_skipped_for_testing() const {
      return last_num_frames_skipped_;
    }
    std::string ToString() const;

   private:
    struct AnimationAdvancementState {
      // The index being displayed on the pending tree.
      size_t pending_index = PaintImage::kDefaultFrameIndex;

      // The time at which we would like to display the next frame. This can be
      // in the past, for instance, if we pause the animation from the image
      // becoming invisible. This time is updated based on either the animation
      // timeline provided by the image (when using Catch-up behavior) or the
      // next displayed frame (when using Resume behavior). Here, "displayed
      // frame" means an animation that updates faster than the display's
      // refresh rate and might skip frames to maintain display speed. See
      // kAnimatedImageResume.
      base::TimeTicks next_desired_frame_time;

      // The time of the next tick at which we want to invalidate and update the
      // current frame.
      base::TimeTicks next_desired_tick_time;

      // The number of loops the animation has finished so far.
      int repetitions_completed = 0;
      size_t num_of_frames_advanced = 0u;
    };

    AnimationAdvancementState AdvanceAnimationState(
        AnimationAdvancementState animation_advancement_state,
        const viz::BeginFrameArgs& args,
        base::TimeTicks start,
        bool enable_image_animation_resync) const;
    void ResetAnimation();
    size_t NextFrameIndex(size_t pending_index) const;
    bool is_complete() const {
      return completion_state_ == PaintImage::CompletionState::kDone;
    }
    bool needs_invalidation() const {
      return current_state_.pending_index != active_index_;
    }
    void ComputeConsistentContentFrameDuration();

    PaintImage::Id paint_image_id_ = PaintImage::kInvalidId;

    // The frame metadata received from the most updated recording with this
    // PaintImage.
    std::vector<FrameMetadata> frames_;

    // The number of animation loops requested for this image. For a value > 0,
    // this number represents the exact number of iterations requested. A few
    // special cases are represented using constants defined in
    // cc/paint/image_animation_count.h
    int requested_repetitions_ = kAnimationNone;

    AnimationAdvancementState current_state_;

    // A set of drivers interested in animating this image.
    base::flat_set<raw_ptr<AnimationDriver, CtnExperimental>> drivers_;

    // The index being used on the active tree, if a recording with this image
    // is still present.
    size_t active_index_ = PaintImage::kDefaultFrameIndex;

    // Cache result for `GetConsistentContentFrameDuration`.
    base::TimeDelta cached_consistent_frame_duration_;
    bool cached_has_consistent_frame_duration_ = false;
    bool cached_consistent_frame_duration_valid_ = false;

    // Set if there is at least one driver interested in animating this image,
    // cached from the last update.
    bool should_animate_from_drivers_ = false;

    // Set if the animation has been started.
    bool animation_started_ = false;
    // Used for tracing, the time at which the animation was started.
    base::TimeTicks animation_started_time_;

    // The last synchronized sequence id for resetting this animation.
    PaintImage::AnimationSequenceId reset_animation_sequence_id_ = 0;

    // Whether the image is known to be completely loaded in the most recent
    // recording received.
    PaintImage::CompletionState completion_state_ =
        PaintImage::CompletionState::kPartiallyDone;

    // The number of frames skipped during catch-up the last time this animation
    // was advanced.
    size_t last_num_frames_skipped_ = 0u;
  };

  class InvalidationScheduler {
   public:
    InvalidationScheduler(base::SingleThreadTaskRunner* task_runner,
                          Client* client);
    ~InvalidationScheduler();

    void Schedule(base::TimeTicks animation_time);
    void Cancel();
    void WillAnimate();
    void WillBeginImplFrame(const viz::BeginFrameArgs& args);
    void set_now_callback_for_testing(NowCallback cb) {
      now_callback_for_testing_ = cb;
    }

   private:
    enum InvalidationState {
      // No notification pending.
      kIdle,
      // Task pending to request impl frame.
      kPendingRequestBeginFrame,
      // Impl frame request pending after request dispatched to client.
      kPendingImplFrame,
      // Sync tree for animation pending after request dispatched to client.
      kPendingInvalidation,
    };

    void RequestBeginFrame();
    void RequestInvalidation();

    raw_ptr<base::SingleThreadTaskRunner> task_runner_;
    const raw_ptr<Client> client_;
    NowCallback now_callback_for_testing_;

    InvalidationState state_ = InvalidationState::kIdle;

    // The time at which the next animation is expected to run.
    base::TimeTicks next_animation_time_;

    base::WeakPtrFactory<InvalidationScheduler> weak_factory_{this};
  };

  // The AnimationState for images is persisted until they are cleared on
  // navigation. This is because while an image might not be painted anymore, if
  // it moves out of the interest rect for instance, the state retained is
  // necessary to resume the animation.
  // TODO(khushalsagar): Implement clearing of state on navigations.
  using AnimationStateMap = base::flat_map<PaintImage::Id, AnimationState>;
  AnimationStateMap animation_state_map_;

  // The set of animations with registered drivers.
  PaintImageIdFlatSet registered_animations_;

  // The set of images that were animated and invalidated on the last sync tree.
  PaintImageIdFlatSet images_animated_on_sync_tree_;

  InvalidationScheduler scheduler_;

  const bool enable_image_animation_resync_;
  const bool use_resume_behavior_;

  bool did_navigate_ = false;
};

}  // namespace cc

#endif  // CC_TREES_IMAGE_ANIMATION_CONTROLLER_H_
