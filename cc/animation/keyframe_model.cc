// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframe_model.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/target_property.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"

namespace cc {

namespace {
#if DCHECK_IS_ON()
int GetNextDebugId() {
  static int g_nextDebugId = 0;
  g_nextDebugId++;
  return g_nextDebugId;
}
#endif
}  // namespace

// static
const KeyframeModel* KeyframeModel::ToCcKeyframeModel(
    const gfx::KeyframeModel* keyframe_model) {
  return static_cast<const KeyframeModel*>(keyframe_model);
}

// static
KeyframeModel* KeyframeModel::ToCcKeyframeModel(
    gfx::KeyframeModel* keyframe_model) {
  return static_cast<KeyframeModel*>(keyframe_model);
}

KeyframeModel::TargetPropertyId::TargetPropertyId(int target_property_type)
    : target_property_type_(target_property_type),
      custom_property_name_(""),
      native_property_type_(PaintWorkletInput::NativePropertyType::kInvalid) {}

KeyframeModel::TargetPropertyId::TargetPropertyId(
    int target_property_type,
    const std::string& custom_property_name)
    : target_property_type_(target_property_type),
      custom_property_name_(custom_property_name),
      native_property_type_(PaintWorkletInput::NativePropertyType::kInvalid) {}

KeyframeModel::TargetPropertyId::TargetPropertyId(
    int target_property_type,
    PaintWorkletInput::NativePropertyType native_property_type)
    : target_property_type_(target_property_type),
      custom_property_name_(""),
      native_property_type_(native_property_type) {}

KeyframeModel::TargetPropertyId::TargetPropertyId(
    const TargetPropertyId& other) = default;

KeyframeModel::TargetPropertyId::TargetPropertyId(TargetPropertyId&& other) =
    default;

KeyframeModel::TargetPropertyId::~TargetPropertyId() = default;

KeyframeModel::TargetPropertyId& KeyframeModel::TargetPropertyId::operator=(
    TargetPropertyId&& other) = default;

std::unique_ptr<KeyframeModel> KeyframeModel::Create(
    std::unique_ptr<gfx::AnimationCurve> curve,
    int keyframe_model_id,
    int group_id,
    TargetPropertyId target_property_id) {
  return base::WrapUnique(new KeyframeModel(std::move(curve), keyframe_model_id,
                                            group_id,
                                            std::move(target_property_id)));
}

std::unique_ptr<KeyframeModel> KeyframeModel::CreateImplInstance(
    RunState initial_run_state) const {
  // Should never clone a model that is the controlling instance as it ends up
  // creating multiple controlling instances.
  DCHECK(!is_controlling_instance_);
  std::unique_ptr<KeyframeModel> to_return(
      new KeyframeModel(curve()->Clone(), id(), group_, target_property_id_));
  to_return->element_id_ = element_id_;
  to_return->ForceRunState(initial_run_state);
  to_return->set_iterations(iterations());
  to_return->set_iteration_start(iteration_start());
  if (has_set_start_time()) {
    to_return->set_start_time(start_time());
  }
  to_return->set_pause_time(pause_time());
  to_return->set_total_paused_duration(total_paused_duration());
  to_return->set_time_offset(time_offset());
  to_return->set_direction(direction());
  to_return->set_playback_rate(playback_rate());
  to_return->set_fill_mode(fill_mode());
  DCHECK(!to_return->is_controlling_instance_);
  to_return->is_controlling_instance_ = true;
#if DCHECK_IS_ON()
  to_return->debug_id_ = debug_id_;
#endif
  return to_return;
}

KeyframeModel::KeyframeModel(std::unique_ptr<gfx::AnimationCurve> curve,
                             int keyframe_model_id,
                             int group_id,
                             TargetPropertyId target_property_id)
    : gfx::KeyframeModel(std::move(curve),
                         keyframe_model_id,
                         target_property_id.target_property_type()),
      group_(group_id),
      target_property_id_(std::move(target_property_id)),
#if DCHECK_IS_ON()
      debug_id_(GetNextDebugId()),
#endif
      needs_synchronized_start_time_(false),
      received_finished_event_(false),
      is_controlling_instance_(false),
      is_impl_only_(false),
      affects_active_elements_(true),
      affects_pending_elements_(true) {
  CHECK_NE(group_, kInvalidGroup);
}

KeyframeModel::~KeyframeModel() = default;

int KeyframeModel::TargetProperty() const {
  return target_property_id_.target_property_type();
}

void KeyframeModel::SetRunState(RunState new_run_state,
                                base::TimeTicks monotonic_time) {
  char name_buffer[256];
  base::snprintf(name_buffer, sizeof(name_buffer), "%s-%d-%d",
                 curve()->TypeName(), TargetProperty(), group_);

  bool is_waiting_to_start =
      run_state() == WAITING_FOR_TARGET_AVAILABILITY || run_state() == STARTING;

  if (is_controlling_instance_ && is_waiting_to_start &&
      new_run_state == RUNNING) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("cc", "KeyframeModel",
                                      TRACE_ID_LOCAL(this), "Name",
                                      TRACE_STR_COPY(name_buffer));
  }

  bool was_finished = is_finished();

  auto old_run_state_name = gfx::KeyframeModel::ToString(run_state());
  gfx::KeyframeModel::SetRunState(new_run_state, monotonic_time);
  auto new_run_state_name = gfx::KeyframeModel::ToString(new_run_state);

  if (is_controlling_instance_ && !was_finished && is_finished()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("cc", "KeyframeModel",
                                    TRACE_ID_LOCAL(this));
  }

  char state_buffer[256];
  base::snprintf(state_buffer, sizeof(state_buffer), "%s->%s",
                 old_run_state_name.c_str(), new_run_state_name.c_str());

  TRACE_EVENT_INSTANT2(
      "cc", "ElementAnimations::SetRunState", TRACE_EVENT_SCOPE_THREAD, "Name",
      TRACE_STR_COPY(name_buffer), "State", TRACE_STR_COPY(state_buffer));
}

bool KeyframeModel::InEffect(base::TimeTicks monotonic_time) const {
  return CalculateActiveTime(monotonic_time).has_value();
}

void KeyframeModel::PushPropertiesTo(KeyframeModel* other) const {
#if DCHECK_IS_ON()
  DCHECK_EQ(debug_id_, other->debug_id_)
      << "Attempted to push properties to a model with a mismatched debug id "
         "(i.e., different keyframe models). This can happen when keyframe "
         "model ids are reused.";
#endif
  other->element_id_ = element_id_;
  if (run_state() == KeyframeModel::PAUSED ||
      other->run_state() == KeyframeModel::PAUSED) {
    other->ForceRunState(run_state());
    other->set_pause_time(pause_time());
    other->set_total_paused_duration(total_paused_duration());
  }
}

std::string KeyframeModel::ToString() const {
  return base::StringPrintf(
      "KeyframeModel{id=%d, group=%d, target_property_type=%d, "
      "custom_property_name=%s, native_property_type=%d, run_state=%s, "
      "element_id=%s}",
      id(), group_, TargetProperty(),
      target_property_id_.custom_property_name().c_str(),
      static_cast<int>(target_property_id_.native_property_type()),
      gfx::KeyframeModel::ToString(run_state()).c_str(),
      element_id_.ToString().c_str());
}

void KeyframeModel::SetIsImplOnly() {
  is_impl_only_ = true;
  // Impl only animations have a single instance which by definition is the
  // controlling instance.
  is_controlling_instance_ = true;
}

bool KeyframeModel::StartShouldBeDeferred() const {
  return needs_synchronized_start_time_;
}

}  // namespace cc
