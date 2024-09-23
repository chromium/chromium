// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_KEYFRAME_MODEL_H_
#define CC_ANIMATION_KEYFRAME_MODEL_H_

#include <memory>
#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_input.h"
#include "ui/gfx/animation/keyframe/keyframe_model.h"

namespace cc {

// A KeyframeModel contains all the state required to play an AnimationCurve.
// Specifically, the affected property, the run state (paused, finished, etc.),
// loop count, last pause time, and the total time spent paused.
// It represents a model of the keyframes (internally represented as a curve).
class CC_ANIMATION_EXPORT KeyframeModel : public gfx::KeyframeModel {
 public:
  static const KeyframeModel* ToCcKeyframeModel(
      const gfx::KeyframeModel* keyframe_model);

  static KeyframeModel* ToCcKeyframeModel(gfx::KeyframeModel* keyframe_model);

  static const int kInvalidGroup = -1;

  // Bundles a property id with its name and native type.
  class CC_ANIMATION_EXPORT TargetPropertyId {
   public:
    // For a property that is neither TargetProperty::CSS_CUSTOM_PROPERTY nor
    // TargetProperty::NATIVE_PROPERTY.
    explicit TargetPropertyId(int target_property_type);
    // For TargetProperty::CSS_CUSTOM_PROPERTY, the string is the custom
    // property name.
    TargetPropertyId(int target_property_type,
                     const std::string& custom_property_name);
    // For TargetProperty::NATIVE_PROPERTY.
    TargetPropertyId(
        int target_property_type,
        PaintWorkletInput::NativePropertyType native_property_type);
    TargetPropertyId(const TargetPropertyId&);
    TargetPropertyId(TargetPropertyId&&);
    ~TargetPropertyId();

    TargetPropertyId& operator=(TargetPropertyId&& other);

    int target_property_type() const { return target_property_type_; }
    const std::string& custom_property_name() const {
      return custom_property_name_;
    }
    PaintWorkletInput::NativePropertyType native_property_type() const {
      return native_property_type_;
    }

   private:
    int target_property_type_;
    // Name of the animated custom property. Empty if it is an animated native
    // property.
    std::string custom_property_name_;
    // Type of the animated native property.
    PaintWorkletInput::NativePropertyType native_property_type_;
  };

  static std::unique_ptr<KeyframeModel> Create(
      std::unique_ptr<gfx::AnimationCurve> curve,
      int keyframe_model_id,
      int group_id,
      TargetPropertyId target_property_id);

  std::unique_ptr<KeyframeModel> CreateImplInstance(
      RunState initial_run_state) const;

  KeyframeModel(const KeyframeModel&) = delete;
  ~KeyframeModel() override;

  KeyframeModel& operator=(const KeyframeModel&) = delete;

  int group() const { return group_; }
  void ungroup() { group_ = kInvalidGroup; }

  int TargetProperty() const override;

  void SetRunState(RunState run_state, base::TimeTicks monotonic_time) override;

  ElementId element_id() const { return element_id_; }
  void set_element_id(ElementId element_id) { element_id_ = element_id; }

  bool InEffect(base::TimeTicks monotonic_time) const;

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
    return target_property_id_.custom_property_name();
  }

  PaintWorkletInput::NativePropertyType native_property_type() const {
    return target_property_id_.native_property_type();
  }

  bool StartShouldBeDeferred() const override;

 private:
  KeyframeModel(std::unique_ptr<gfx::AnimationCurve> curve,
                int keyframe_model_id,
                int group_id,
                TargetPropertyId target_property_id);
  // KeyframeModels that must be run together are called 'grouped' and have the
  // same group id. Grouped KeyframeModels are guaranteed to start at the same
  // time and no other KeyframeModels may animate any of the group's target
  // properties until all KeyframeModels in the group have finished animating.
  int group_;

  TargetPropertyId target_property_id_;

  // If specified, overrides the ElementId to apply this KeyframeModel's effect
  // value on.
  ElementId element_id_;

#if DCHECK_IS_ON()
  // This id is unique, modulo overflow. Permits quick instance equality checks.
  int debug_id_ = 0;
#endif

  bool needs_synchronized_start_time_;
  bool received_finished_event_;

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
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAME_MODEL_H_
