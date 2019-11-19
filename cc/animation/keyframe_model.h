// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_KEYFRAME_MODEL_H_
#define CC_ANIMATION_KEYFRAME_MODEL_H_

#include <memory>

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/paint/element_id.h"

namespace cc {

class AnimationCurve;

// A KeyframeModel contains all the state required to play an AnimationCurve.
// Specifically, the affected property, the run state (paused, finished, etc.),
// loop count, last pause time, and the total time spent paused.
// It represents a model of the keyframes (internally represented as a curve).
class CC_ANIMATION_EXPORT KeyframeModel {
 public:
  // KeyframeModels begin in the 'WAITING_FOR_TARGET_AVAILABILITY' state. A
  // KeyframeModel waiting for target availibility will run as soon as its
  // target property is free (and all the KeyframeModels animating with it are
  // also able to run). When this time arrives, the controller will move the
  // keyframe model into the STARTING state, and then into the RUNNING state.
  // RUNNING KeyframeModels may toggle between RUNNING and PAUSED, and may be
  // stopped by moving into either the ABORTED or FINISHED states. A FINISHED
  // keyframe model was allowed to run to completion, but an ABORTED keyframe
  // model was not. An animation in the state ABORTED_BUT_NEEDS_COMPLETION is a
  // keyframe model that was aborted for some reason, but needs to be finished.
  // Currently this is for impl-only scroll offset KeyframeModels that need to
  // be completed on the main thread.
  enum RunState {
    WAITING_FOR_TARGET_AVAILABILITY = 0,
    WAITING_FOR_DELETION,
    STARTING,
    RUNNING,
    PAUSED,
    FINISHED,
    ABORTED,
    ABORTED_BUT_NEEDS_COMPLETION,
    // This sentinel must be last.
    LAST_RUN_STATE = ABORTED_BUT_NEEDS_COMPLETION
  };
  static std::string ToString(RunState);

  enum class Direction { NORMAL, REVERSE, ALTERNATE_NORMAL, ALTERNATE_REVERSE };

  enum class FillMode { NONE, FORWARDS, BACKWARDS, BOTH, AUTO };

  enum class Phase { BEFORE, ACTIVE, AFTER };

  // The |custom_property_name| has a default value of an empty string,
  // indicating that the animated property is a native property. When it is an
  // animated custom property, it should be the property name.
  static std::unique_ptr<KeyframeModel> Create(
      std::unique_ptr<AnimationCurve> curve,
      int keyframe_model_id,
      int group_id,
      int target_property_id,
      const std::string& custom_property_name = "");

  std::unique_ptr<KeyframeModel> CreateImplInstance(
      RunState initial_run_state) const;

  KeyframeModel(const KeyframeModel&) = delete;
  virtual ~KeyframeModel();

  KeyframeModel& operator=(const KeyframeModel&) = delete;

  int id() const { return id_; }
  int group() const { return group_; }
  int target_property_id() const { return target_property_id_; }

  ElementId element_id() const { return element_id_; }
  void set_element_id(ElementId element_id) { element_id_ = element_id; }

  RunState run_state() const { return run_state_; }
  void SetRunState(RunState run_state, base::TimeTicks monotonic_time);

  // This is the number of times that the keyframe model will play. If this
  // value is zero the keyframe model will not play. If it is negative, then
  // the keyframe model will loop indefinitely.
  double iterations() const { return iterations_; }
  void set_iterations(double n) { iterations_ = n; }

  double iteration_start() const { return iteration_start_; }
  void set_iteration_start(double iteration_start) {
    iteration_start_ = iteration_start;
  }

  base::TimeTicks start_time() const { return start_time_; }

  void set_start_time(base::TimeTicks monotonic_time) {
    start_time_ = monotonic_time;
  }
  bool has_set_start_time() const { return !start_time_.is_null(); }

  base::TimeDelta time_offset() const { return time_offset_; }
  void set_time_offset(base::TimeDelta monotonic_time) {
    time_offset_ = monotonic_time;
  }

  // Pause the keyframe effect at local time |pause_offset|.
  void Pause(base::TimeDelta pause_offset);

  Direction direction() { return direction_; }
  void set_direction(Direction direction) { direction_ = direction; }

  FillMode fill_mode() { return fill_mode_; }
  void set_fill_mode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  double playback_rate() { return playback_rate_; }
  void set_playback_rate(double playback_rate) {
    playback_rate_ = playback_rate;
  }

  bool IsFinishedAt(base::TimeTicks monotonic_time) const;
  bool is_finished() const {
    return run_state_ == FINISHED || run_state_ == ABORTED ||
           run_state_ == WAITING_FOR_DELETION;
  }

  bool InEffect(base::TimeTicks monotonic_time) const;

  AnimationCurve* curve() { return curve_.get(); }
  const AnimationCurve* curve() const { return curve_.get(); }

  // If this is true, even if the keyframe model is running, it will not be
  // tickable until it is given a start time. This is true for KeyframeModels
  // running on the main thread.
  bool needs_synchronized_start_time() const {
    return needs_synchronized_start_time_;
  }
  void set_needs_synchronized_start_time(bool needs_synchronized_start_time) {
    needs_synchronized_start_time_ = needs_synchronized_start_time;
  }

  // This is true for KeyframeModels running on the main thread when the
  // FINISHED event sent by the corresponding impl keyframe model has been
  // received.
  bool received_finished_event() const { return received_finished_event_; }
  void set_received_finished_event(bool received_finished_event) {
    received_finished_event_ = received_finished_event;
  }

  // Takes the given absolute time, and using the start time and the number
  // of iterations, returns the relative time in the current iteration.
  base::TimeDelta TrimTimeToCurrentIteration(
      base::TimeTicks monotonic_time) const;

  void set_is_controlling_instance_for_test(bool is_controlling_instance) {
    is_controlling_instance_ = is_controlling_instance;
  }
  bool is_controlling_instance() const { return is_controlling_instance_; }

  void PushPropertiesTo(KeyframeModel* other) const;

  std::string ToString() const;

  void SetIsImplOnly();
  bool is_impl_only() const { return is_impl_only_; }

  void set_affects_active_elements(bool affects_active_elements) {
    affects_active_elements_ = affects_active_elements;
  }
  bool affects_active_elements() const { return affects_active_elements_; }

  void set_affects_pending_elements(bool affects_pending_elements) {
    affects_pending_elements_ = affects_pending_elements;
  }
  bool affects_pending_elements() const { return affects_pending_elements_; }

  const std::string& custom_property_name() const {
    return custom_property_name_;
  }

  KeyframeModel::Phase CalculatePhaseForTesting(
      base::TimeDelta local_time) const;

 private:
  KeyframeModel(std::unique_ptr<AnimationCurve> curve,
                int keyframe_model_id,
                int group_id,
                int target_property_id,
                const std::string& custom_property_name);

  // Return local time for this keyframe model given the absolute monotonic
  // time.
  //
  // Local time represents the time value that is used to tick this keyframe
  // model and is relative to its start time. It is closely related to the local
  // time concept in web animations [1]. It is:
  //  - for playing animation : wall time - start time - paused duration
  //  - for paused animation  : paused time
  //  - otherwise             : zero
  //
  // Here is small diagram that shows how active, local, and monotonic times
  // relate to each other and to the run state.
  //
  //      run state   Starting  (R)unning  Paused (R) Paused (R)  Finished
  //                    ^                                          ^
  //                    |                                          |
  // monotonic time  ------------------------------------------------->
  //                    |                                          |
  //     local time     +-----------------+      +---+      +--------->
  //                    |                                          |
  //    active time     +          +------+      +---+      +------+
  //                      (-offset)
  //
  // [1] https://drafts.csswg.org/web-animations/#local-time-section
  base::TimeDelta ConvertMonotonicTimeToLocalTime(
      base::TimeTicks monotonic_time) const;

  KeyframeModel::Phase CalculatePhase(base::TimeDelta local_time) const;
  base::Optional<base::TimeDelta> CalculateActiveTime(
      base::TimeTicks monotonic_time) const;

  std::unique_ptr<AnimationCurve> curve_;

  // IDs must be unique.
  int id_;

  // KeyframeModels that must be run together are called 'grouped' and have the
  // same group id. Grouped KeyframeModels are guaranteed to start at the same
  // time and no other KeyframeModels may animate any of the group's target
  // properties until all KeyframeModels in the group have finished animating.
  int group_;

  // If specified, overrides the ElementId to apply this KeyframeModel's effect
  // value on.
  ElementId element_id_;

  int target_property_id_;
  RunState run_state_;
  double iterations_;
  double iteration_start_;
  base::TimeTicks start_time_;
  Direction direction_;
  double playback_rate_;
  FillMode fill_mode_;

  // The time offset effectively pushes the start of the keyframe model back in
  // time. This is used for resuming paused KeyframeModels -- an animation is
  // added with a non-zero time offset, causing the keyframe model to skip ahead
  // to the desired point in time.
  base::TimeDelta time_offset_;

  bool needs_synchronized_start_time_;
  bool received_finished_event_;

  // These are used when converting monotonic to local time to account for time
  // spent while paused. This is not included in AnimationState since it
  // there is absolutely no need for clients of this controller to know
  // about these values.
  base::TimeTicks pause_time_;
  base::TimeDelta total_paused_duration_;

  // KeyframeModels lead dual lives. An active keyframe model will be
  // conceptually owned by two controllers, one on the impl thread and one on
  // the main. In reality, there will be two separate KeyframeModel instances
  // for the same keyframe model. They will have the same group id and the same
  // target property (these two values uniquely identify a keyframe model). The
  // instance on the impl thread is the instance that ultimately controls the
  // values of the animating layer and so we will refer to it as the
  // 'controlling instance'.
  // Impl only keyframe models are the exception to this rule. They have only a
  // single instance. We consider this instance as the 'controlling instance'.
  bool is_controlling_instance_;

  bool is_impl_only_;

  // When pushed from a main-thread controller to a compositor-thread
  // controller, a keyframe model will initially only affect pending elements
  // (corresponding to layers in the pending tree). KeyframeModels that only
  // affect pending elements are able to reach the STARTING state and tick
  // pending elements, but cannot proceed any further and do not tick active
  // elements. After activation, such KeyframeModels affect both kinds of
  // elements and are able to proceed past the STARTING state. When the removal
  // of a keyframe model is pushed from a main-thread controller to a
  // compositor-thread controller, this initially only makes the keyframe model
  // stop affecting pending elements. After activation, such KeyframeModels no
  // longer affect any elements, and are deleted.
  bool affects_active_elements_;
  bool affects_pending_elements_;

  // Name of the animated custom property. Empty if it is an animated native
  // property.
  std::string custom_property_name_;
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAME_MODEL_H_
