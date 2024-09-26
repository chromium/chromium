// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframe_effect.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/base/features.h"
#include "cc/trees/property_animation_state.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/target_property.h"
#include "ui/gfx/geometry/transform_operations.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

namespace {

bool NeedsFinishedEvent(KeyframeModel* keyframe_model) {
  // The controlling instance (i.e., impl instance), sends the finish event and
  // does not need to receive it.
  if (keyframe_model->is_controlling_instance())
    return false;

  return !keyframe_model->received_finished_event();
}

// Returns indices for keyframe_models that have matching group id.
std::vector<size_t> FindAnimationsWithSameGroupId(
    const std::vector<std::unique_ptr<gfx::KeyframeModel>>& keyframe_models,
    int group_id) {
  std::vector<size_t> group;
  for (size_t i = 0; i < keyframe_models.size(); ++i) {
    auto* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_models[i].get());
    if (cc_keyframe_model->group() != group_id)
      continue;
    group.push_back(i);
  }
  return group;
}

}  // namespace

KeyframeEffect::KeyframeEffect(Animation* animation)
    : animation_(animation),
      element_animations_(),
      needs_to_start_keyframe_models_(false),
      scroll_offset_animation_was_interrupted_(false),
      is_ticking_(false),
      awaiting_deletion_(false),
      needs_push_properties_(false) {}

KeyframeEffect::~KeyframeEffect() {
  DCHECK(!has_bound_element_animations());
}

void KeyframeEffect::SetNeedsPushProperties() {
  needs_push_properties_ = true;

  // The keyframe effect may have been removed from the main thread while
  // an event was in flight from the compositor. In this case, we may need
  // to push the removal to the compositor but do not expect to have a bound
  // element animations instance.
  // TODO(smcgruer): We only need the below calls when needs_push_properties_
  // goes from false to true - see http://crbug.com/764405
  if (element_animations()) {
    element_animations_->SetNeedsPushProperties();
  }

  animation_->SetNeedsPushProperties();
}

void KeyframeEffect::ResetNeedsPushProperties() {
  needs_push_properties_ = false;
}

void KeyframeEffect::BindElementAnimations(
    ElementAnimations* element_animations) {
  DCHECK(element_animations);
  DCHECK(!element_animations_);
  element_animations_ = element_animations;

  DCHECK(element_id_);
  DCHECK(element_id_ == element_animations->element_id());

  if (has_any_keyframe_model())
    KeyframeModelAdded();
  SetNeedsPushProperties();
}

void KeyframeEffect::UnbindElementAnimations() {
  SetNeedsPushProperties();
  element_animations_ = nullptr;
}

void KeyframeEffect::AttachElement(ElementId element_id) {
  DCHECK(!element_id_);
  DCHECK(element_id);
  element_id_ = element_id;
}

void KeyframeEffect::DetachElement() {
  DCHECK(element_id_);
  element_id_ = ElementId();
}

bool KeyframeEffect::Tick(base::TimeTicks monotonic_time) {
  DCHECK(has_bound_element_animations());
  if (needs_to_start_keyframe_models_)
    StartKeyframeModels(monotonic_time);

  bool became_inactive = false;
  bool is_effect_active = false;
  for (auto& keyframe_model : keyframe_models()) {
    TickKeyframeModel(monotonic_time, keyframe_model.get());
    bool was_active = last_tick_time_.has_value() &&
                      keyframe_model->HasActiveTime(*last_tick_time_);
    bool is_active = keyframe_model->HasActiveTime(monotonic_time);
    is_effect_active |= is_active;
    became_inactive |= (was_active && !is_active);
  }

  last_tick_time_ = monotonic_time;
  element_animations_->UpdateClientAnimationState();
  if (became_inactive) {
    animation_->SetNeedsCommit();
  }
  return is_effect_active;
}

void KeyframeEffect::RemoveFromTicking() {
  is_ticking_ = false;
  // Resetting last_tick_time_ here ensures that calling ::UpdateState
  // before ::Animate doesn't start a keyframe model.
  last_tick_time_ = std::nullopt;
  animation_->RemoveFromTicking();
}

void KeyframeEffect::UpdateState(bool start_ready_keyframe_models,
                                 AnimationEvents* events) {
  DCHECK(has_bound_element_animations());

  // Animate hasn't been called, this happens if an element has been added
  // between the Commit and Draw phases.
  if (last_tick_time_ == std::nullopt || awaiting_deletion_) {
    start_ready_keyframe_models = false;
  }

  if (start_ready_keyframe_models)
    PromoteStartedKeyframeModels(events);

  auto last_tick_time = last_tick_time_.value_or(base::TimeTicks());
  MarkFinishedKeyframeModels(last_tick_time);
  MarkKeyframeModelsForDeletion(last_tick_time, events);
  PurgeKeyframeModelsMarkedForDeletion(/* impl_only */ true);

  if (start_ready_keyframe_models) {
    if (needs_to_start_keyframe_models_) {
      StartKeyframeModels(last_tick_time);
      PromoteStartedKeyframeModels(events);
    }
  }
}

void KeyframeEffect::UpdateTickingState() {
  if (!animation_->has_animation_host()) {
    return;
  }
  bool was_ticking = is_ticking_;
  is_ticking_ = false;

  for (const auto& keyframe_model : keyframe_models()) {
    if (keyframe_model->run_state() !=
        gfx::KeyframeModel::WAITING_FOR_DELETION) {
      is_ticking_ = true;
      awaiting_deletion_ = false;
      break;
    }
  }
  if (was_ticking && !is_ticking_) {
    awaiting_deletion_ = false;
    if (base::FeatureList::IsEnabled(features::kNoPreserveLastMutation)) {
      for (const auto& keyframe_model : keyframe_models()) {
        // deleted impl side keyframe models keep ticking until the commit
        // removes them.
        KeyframeModel* cc_keyframe_model =
            KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
        if (keyframe_model->run_state() ==
                gfx::KeyframeModel::WAITING_FOR_DELETION &&
            cc_keyframe_model->is_controlling_instance() &&
            !cc_keyframe_model->is_impl_only()) {
          awaiting_deletion_ = true;
          is_ticking_ = true;
          break;
        }
      }
    }
  }

  if (is_ticking_ && !was_ticking) {
    animation_->AddToTicking();
  } else if (!is_ticking_ && was_ticking) {
    RemoveFromTicking();
  }
}

void KeyframeEffect::Pause(base::TimeTicks timeline_time,
                           PauseCondition pause_condition) {
  bool did_pause = false;
  for (auto& keyframe_model : keyframe_models()) {
    // TODO(crbug.com/40688021): KeyframeEffect is paused with local time for
    // scroll-linked animations. To make sure the start event of a keyframe
    // model is sent to blink, we should not set its run state to PAUSED until
    // such event is sent. This should be revisited once KeyframeEffect is able
    // to tick scroll-linked keyframe models directly.
    if (pause_condition == PauseCondition::kAfterStart &&
        (keyframe_model->run_state() ==
             gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY ||
         keyframe_model->run_state() == gfx::KeyframeModel::STARTING))
      continue;
    // Convert the timeline_time to the effective local time for each
    // KeyframeModel's start time.
    keyframe_model->Pause(timeline_time - keyframe_model->start_time());
    did_pause = true;
  }

  if (!did_pause || !has_bound_element_animations())
    return;
  animation_->SetNeedsCommit();
  SetNeedsPushProperties();
}

void KeyframeEffect::AddKeyframeModel(
    std::unique_ptr<gfx::KeyframeModel> keyframe_model) {
  KeyframeModel* cc_keyframe_model =
      KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
  DCHECK(!cc_keyframe_model->is_impl_only() ||
         keyframe_model->TargetProperty() == TargetProperty::SCROLL_OFFSET);
  // This is to make sure that keyframe models in the same group, i.e., start
  // together, don't animate the same property.
  DCHECK(base::ranges::none_of(
      keyframe_models(), [&](const auto& existing_keyframe_model) {
        auto* cc_existing_keyframe_model =
            KeyframeModel::ToCcKeyframeModel(existing_keyframe_model.get());
        bool same_group_and_target =
            keyframe_model->TargetProperty() ==
                existing_keyframe_model->TargetProperty() &&
            cc_keyframe_model->group() == cc_existing_keyframe_model->group();
        // Keyframe models in the same group might target the same property
        // if one or both is an outgoing animation (i.e. about to be
        // removed).
        bool both_active =
            cc_keyframe_model->affects_active_elements() &&
            cc_existing_keyframe_model->affects_active_elements();
        bool both_pending =
            cc_keyframe_model->affects_pending_elements() &&
            cc_existing_keyframe_model->affects_pending_elements();
        return same_group_and_target && (both_active || both_pending);
      }));

  if (keyframe_model->TargetProperty() == TargetProperty::SCROLL_OFFSET) {
    // We should never have more than one scroll offset animation queued on the
    // same scrolling element as this would result in multiple automated
    // scrolls.
    DCHECK(base::ranges::none_of(
        keyframe_models(), [&](const auto& existing_keyframe_model) {
          auto* cc_existing_keyframe_model =
              KeyframeModel::ToCcKeyframeModel(existing_keyframe_model.get());
          return cc_existing_keyframe_model->TargetProperty() ==
                     TargetProperty::SCROLL_OFFSET &&
                 !cc_existing_keyframe_model->is_finished() &&
                 (!cc_existing_keyframe_model->is_controlling_instance() ||
                  cc_existing_keyframe_model->affects_pending_elements());
        }));
  }

  gfx::KeyframeEffect::AddKeyframeModel(std::move(keyframe_model));

  if (has_bound_element_animations()) {
    KeyframeModelAdded();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::PauseKeyframeModel(int keyframe_model_id,
                                        base::TimeDelta time_offset) {
  for (auto& keyframe_model : keyframe_models()) {
    if (keyframe_model->id() == keyframe_model_id) {
      keyframe_model->Pause(time_offset);
    }
  }

  if (has_bound_element_animations()) {
    animation_->SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::AbortKeyframeModel(int keyframe_model_id) {
  if (gfx::KeyframeModel* keyframe_model =
          GetKeyframeModelById(keyframe_model_id)) {
    if (!keyframe_model->is_finished()) {
      keyframe_model->SetRunState(gfx::KeyframeModel::ABORTED,
                                  last_tick_time_.value_or(base::TimeTicks()));
      if (has_bound_element_animations())
        element_animations_->UpdateClientAnimationState();
    }
  }

  if (has_bound_element_animations()) {
    animation_->SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::AbortKeyframeModelsWithProperty(
    TargetProperty::Type target_property,
    bool needs_completion) {
  if (needs_completion)
    DCHECK(target_property == TargetProperty::SCROLL_OFFSET);

  bool aborted_keyframe_model = false;
  for (auto& keyframe_model : keyframe_models()) {
    if (keyframe_model->TargetProperty() == target_property &&
        !keyframe_model->is_finished()) {
      // Currently only impl-only scroll offset KeyframeModels can be completed
      // on the main thread.
      if (needs_completion &&
          KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
              ->is_impl_only()) {
        keyframe_model->SetRunState(
            gfx::KeyframeModel::ABORTED_BUT_NEEDS_COMPLETION,
            last_tick_time_.value_or(base::TimeTicks()));
      } else {
        keyframe_model->SetRunState(
            gfx::KeyframeModel::ABORTED,
            last_tick_time_.value_or(base::TimeTicks()));
      }
      aborted_keyframe_model = true;
    }
  }

  if (has_bound_element_animations()) {
    if (aborted_keyframe_model)
      element_animations_->UpdateClientAnimationState();
    animation_->SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::ActivateKeyframeModels() {
  DCHECK(has_bound_element_animations());

  bool keyframe_model_activated = false;
  for (auto& keyframe_model : keyframe_models()) {
    auto* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get());

    if (replaced_group_ == cc_keyframe_model->group() &&
        !cc_keyframe_model->affects_pending_elements()) {
      CHECK_NE(cc_keyframe_model->group(), KeyframeModel::kInvalidGroup);
      cc_keyframe_model->ungroup();
    }

    if (cc_keyframe_model->affects_active_elements() !=
        cc_keyframe_model->affects_pending_elements()) {
      keyframe_model_activated = true;
    }
    cc_keyframe_model->set_affects_active_elements(
        cc_keyframe_model->affects_pending_elements());
  }

  if (keyframe_model_activated)
    element_animations_->UpdateClientAnimationState();

  replaced_group_.reset();

  scroll_offset_animation_was_interrupted_ = false;
}

void KeyframeEffect::KeyframeModelAdded() {
  DCHECK(has_bound_element_animations());

  animation_->SetNeedsCommit();
  needs_to_start_keyframe_models_ = true;

  UpdateTickingState();
  for (auto& keyframe_model : keyframe_models()) {
    element_animations_->AttachToCurve(keyframe_model->curve());
  }
  element_animations_->UpdateClientAnimationState();
}

bool KeyframeEffect::DispatchAnimationEventToKeyframeModel(
    const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  KeyframeModel* keyframe_model = KeyframeModel::ToCcKeyframeModel(
      GetKeyframeModelById(event.uid.model_id));
  bool dispatched = false;
  switch (event.type) {
    case AnimationEvent::Type::kStarted:
      if (!keyframe_model) {
        KeyframeModel* replacement = KeyframeModel::ToCcKeyframeModel(
            GetKeyframeModel(event.target_property));
        if (replacement && replacement->group() == event.group_id) {
          keyframe_model = replacement;
        }
      }
      if (keyframe_model && keyframe_model->needs_synchronized_start_time()) {
        keyframe_model->set_needs_synchronized_start_time(false);
        if (!keyframe_model->has_set_start_time())
          keyframe_model->set_start_time(event.monotonic_time);
        dispatched = true;
      }
      break;

    case AnimationEvent::Type::kFinished:
      if (keyframe_model) {
        keyframe_model->set_received_finished_event(true);
        dispatched = true;
      } else {
        // This is for the case when a keyframe_model is already removed on main
        // thread, but the impl version of it sent a finished event and is now
        // waiting for deletion. We would need to delete that keyframe_model
        // during push properties.
        SetNeedsPushProperties();
      }
      break;

    case AnimationEvent::Type::kAborted:
      if (keyframe_model) {
        keyframe_model->SetRunState(gfx::KeyframeModel::ABORTED,
                                    event.monotonic_time);
        keyframe_model->set_received_finished_event(true);
        dispatched = true;
        animation_->animation_host()
            ->UpdateClientAnimationStateForElementAnimations(element_id());
      }
      break;

    case AnimationEvent::Type::kTakeOver:
      // TODO(crbug.com/40655283): Routing TAKEOVER events is broken.
      // We need to purge KeyframeModels marked for deletion on CT.
      SetNeedsPushProperties();
      dispatched = true;
      break;

    case AnimationEvent::Type::kTimeUpdated:
      // TIME_UPDATED events are used to synchronize effect time between cc and
      // main thread worklet animations. Keyframe models are not involved in
      // this process.
      NOTREACHED();
  }
  return dispatched;
}

bool KeyframeEffect::HasTickingKeyframeModel() const {
  for (const auto& keyframe_model : keyframe_models()) {
    if (!keyframe_model->is_finished())
      return true;
  }
  return false;
}

bool KeyframeEffect::RequiresInvalidation() const {
  for (const auto& it : keyframe_models()) {
    if (it->TargetProperty() == TargetProperty::NATIVE_PROPERTY ||
        it->TargetProperty() == TargetProperty::CSS_CUSTOM_PROPERTY) {
      return true;
    }
  }
  return false;
}

bool KeyframeEffect::AffectsNativeProperty() const {
  for (const auto& it : keyframe_models()) {
    // TODO(crbug.com/40796582): include the SCROLL_OFFSET here so that we won't
    // create a compositor animation frame sequence tracker when there is a
    // composited scroll.
    if (it->TargetProperty() != TargetProperty::CSS_CUSTOM_PROPERTY &&
        it->TargetProperty() != TargetProperty::NATIVE_PROPERTY)
      return true;
  }
  return false;
}

bool KeyframeEffect::AnimationsPreserveAxisAlignment() const {
  for (const auto& keyframe_model : keyframe_models()) {
    if (keyframe_model->is_finished())
      continue;

    if (!keyframe_model->curve()->PreservesAxisAlignment())
      return false;
  }
  return true;
}

float KeyframeEffect::MaximumScale(ElementId element_id,
                                   ElementListType list_type) const {
  float maximum_scale = kInvalidScale;
  for (const auto& keyframe_model : keyframe_models()) {
    if (keyframe_model->is_finished())
      continue;

    auto* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get());

    ElementId model_element_id = cc_keyframe_model->element_id();
    if (!model_element_id)
      model_element_id = element_id_;
    if (model_element_id != element_id)
      continue;

    if ((list_type == ElementListType::ACTIVE &&
         !cc_keyframe_model->affects_active_elements()) ||
        (list_type == ElementListType::PENDING &&
         !cc_keyframe_model->affects_pending_elements()))
      continue;

    float curve_maximum_scale = kInvalidScale;
    if (keyframe_model->curve()->MaximumScale(&curve_maximum_scale))
      maximum_scale = std::max(maximum_scale, curve_maximum_scale);
  }
  return maximum_scale;
}

bool KeyframeEffect::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (const auto& keyframe_model : keyframe_models()) {
    if (!keyframe_model->is_finished() &&
        keyframe_model->TargetProperty() == target_property) {
      auto* cc_keyframe_model =
          KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
      if ((list_type == ElementListType::ACTIVE &&
           cc_keyframe_model->affects_active_elements()) ||
          (list_type == ElementListType::PENDING &&
           cc_keyframe_model->affects_pending_elements()))
        return true;
    }
  }
  return false;
}

bool KeyframeEffect::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (const auto& keyframe_model : keyframe_models()) {
    auto* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
    if (!keyframe_model->is_finished() &&
        cc_keyframe_model->InEffect(
            last_tick_time_.value_or(base::TimeTicks())) &&
        keyframe_model->TargetProperty() == target_property) {
      if ((list_type == ElementListType::ACTIVE &&
           cc_keyframe_model->affects_active_elements()) ||
          (list_type == ElementListType::PENDING &&
           cc_keyframe_model->affects_pending_elements()))
        return true;
    }
  }
  return false;
}

void KeyframeEffect::GetPropertyAnimationState(
    PropertyAnimationState* pending_state,
    PropertyAnimationState* active_state) const {
  pending_state->Clear();
  active_state->Clear();

  for (const auto& keyframe_model : keyframe_models()) {
    if (!keyframe_model->is_finished()) {
      auto* cc_keyframe_model =
          KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
      bool in_effect = cc_keyframe_model->InEffect(
          last_tick_time_.value_or(base::TimeTicks()));
      bool active = cc_keyframe_model->affects_active_elements();
      bool pending = cc_keyframe_model->affects_pending_elements();
      int property = keyframe_model->TargetProperty();

      if (pending)
        pending_state->potentially_animating[property] = true;
      if (pending && in_effect)
        pending_state->currently_running[property] = true;

      if (active)
        active_state->potentially_animating[property] = true;
      if (active && in_effect)
        active_state->currently_running[property] = true;
    }
  }
}

void KeyframeEffect::MarkAbortedKeyframeModelsForDeletion(
    KeyframeEffect* keyframe_effect_impl) {
  bool keyframe_model_aborted = false;

  auto& keyframe_models_impl = keyframe_effect_impl->keyframe_models();
  for (const auto& keyframe_model_impl : keyframe_models_impl) {
    // If the keyframe_model has been aborted on the main thread, mark it for
    // deletion.
    if (gfx::KeyframeModel* keyframe_model =
            GetKeyframeModelById(keyframe_model_impl->id())) {
      if (keyframe_model->run_state() == gfx::KeyframeModel::ABORTED) {
        keyframe_model_impl->SetRunState(
            gfx::KeyframeModel::WAITING_FOR_DELETION,
            keyframe_effect_impl->last_tick_time_.value_or(base::TimeTicks()));
        keyframe_model->SetRunState(
            gfx::KeyframeModel::WAITING_FOR_DELETION,
            last_tick_time_.value_or(base::TimeTicks()));
        keyframe_model_aborted = true;
      }
    }
  }

  if (has_bound_element_animations() && keyframe_model_aborted)
    element_animations_->SetNeedsPushProperties();
}

void KeyframeEffect::PurgeKeyframeModelsMarkedForDeletion(bool impl_only) {
  std::erase_if(keyframe_models(), [impl_only](const auto& keyframe_model) {
    return keyframe_model->run_state() ==
               gfx::KeyframeModel::WAITING_FOR_DELETION &&
           (!impl_only || KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
                              ->is_impl_only());
  });
}

void KeyframeEffect::PurgeDeletedKeyframeModels() {
  std::erase_if(keyframe_models(), [](const auto& keyframe_model) {
    return keyframe_model->run_state() ==
               gfx::KeyframeModel::WAITING_FOR_DELETION &&
           !KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
                ->affects_pending_elements();
  });
}

void KeyframeEffect::PushNewKeyframeModelsToImplThread(
    KeyframeEffect* keyframe_effect_impl) const {
  // Any new KeyframeModels owned by the main thread's Animation are
  // cloned and added to the impl thread's Animation.
  for (const auto& keyframe_model : keyframe_models()) {
    // If the keyframe_model is finished, do not copy it over to impl since the
    // impl instance, if there was one, was just removed in
    // |RemoveKeyframeModelsCompletedOnMainThread|.
    if (keyframe_model->is_finished())
      continue;
    // If the keyframe_model is already running on the impl thread, there is no
    // need to copy it over.
    if (keyframe_effect_impl->GetKeyframeModelById(keyframe_model->id()))
      continue;

    if (keyframe_model->TargetProperty() == TargetProperty::SCROLL_OFFSET &&
        !ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
             keyframe_model->curve())
             ->HasSetInitialValue()) {
      std::optional<gfx::PointF> current_scroll_offset;
      // If the scroller was already composited, prefer using its current scroll
      // offset.
      current_scroll_offset = keyframe_effect_impl->ScrollOffsetForAnimation();
      // Otherwise, take the scroll offset from the commit with the animation.
      if (!current_scroll_offset.has_value())
        current_scroll_offset = ScrollOffsetForAnimation();
      DCHECK(current_scroll_offset);
      ScrollOffsetAnimationCurve* curve =
          ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
              keyframe_model->curve());
      curve->SetInitialValue(*current_scroll_offset);
    }

    // The new keyframe_model should be set to run as soon as possible.
    gfx::KeyframeModel::RunState initial_run_state =
        gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY;
    std::unique_ptr<KeyframeModel> to_add(
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
            ->CreateImplInstance(initial_run_state));
    DCHECK(!to_add->needs_synchronized_start_time());
    to_add->set_affects_active_elements(false);
    keyframe_effect_impl->AddKeyframeModel(std::move(to_add));
  }
}

namespace {
bool IsCompleted(gfx::KeyframeModel* keyframe_model,
                 const KeyframeEffect* main_thread_keyframe_effect) {
  if (KeyframeModel::ToCcKeyframeModel(keyframe_model)->is_impl_only()) {
    return (keyframe_model->run_state() ==
            gfx::KeyframeModel::WAITING_FOR_DELETION);
  } else {
    gfx::KeyframeModel* main_thread_keyframe_model =
        main_thread_keyframe_effect->GetKeyframeModelById(keyframe_model->id());
    return !main_thread_keyframe_model ||
           main_thread_keyframe_model->is_finished();
  }
}
}  // namespace

void KeyframeEffect::RemoveKeyframeModelsCompletedOnMainThread(
    KeyframeEffect* keyframe_effect_impl) const {
  bool keyframe_model_completed = false;

  // Animations removed on the main thread should no longer affect pending
  // elements, and should stop affecting active elements after the next call
  // to ActivateKeyframeEffects. If already WAITING_FOR_DELETION, they can be
  // removed immediately.
  for (auto& keyframe_model : keyframe_effect_impl->keyframe_models()) {
    if (IsCompleted(keyframe_model.get(), this)) {
      KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
          ->set_affects_pending_elements(false);
      keyframe_model_completed = true;
    }
  }

  keyframe_effect_impl->PurgeDeletedKeyframeModels();
  if (has_bound_element_animations() && keyframe_model_completed)
    element_animations_->SetNeedsPushProperties();
}

void KeyframeEffect::PushPropertiesTo(
    KeyframeEffect* keyframe_effect_impl,
    std::optional<base::TimeTicks> replaced_start_time) {
  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  keyframe_effect_impl->SetNeedsPushProperties();

  // Synchronize the keyframe_model target between main and impl side.
  if (element_id_ != keyframe_effect_impl->element_id_) {
    // We have to detach/attach via the Animation as it may need to inform
    // the host as well.
    if (keyframe_effect_impl->has_attached_element())
      keyframe_effect_impl->animation_->DetachElement();
    if (element_id_) {
      if (element_id_ == kReservedElementIdForPaintWorklet) {
        keyframe_effect_impl->animation_->AttachPaintWorkletElement();
      } else {
        keyframe_effect_impl->animation_->AttachElement(element_id_);
      }
    }
  }

  keyframe_effect_impl->scroll_offset_animation_was_interrupted_ =
      scroll_offset_animation_was_interrupted_;
  scroll_offset_animation_was_interrupted_ = false;

  // If neither main nor impl have any KeyframeModels, there is nothing further
  // to synchronize.
  if (!has_any_keyframe_model() &&
      !keyframe_effect_impl->has_any_keyframe_model())
    return;

  if (replaced_start_time) {
    for (auto& km : keyframe_models()) {
      km->set_start_time(*replaced_start_time);
    }
  }

  // Synchronize the main-thread and impl-side keyframe model lists, removing
  // aborted KeyframeModels and pushing any new animations.
  MarkAbortedKeyframeModelsForDeletion(keyframe_effect_impl);
  PurgeKeyframeModelsMarkedForDeletion(/* impl_only */ false);
  RemoveKeyframeModelsCompletedOnMainThread(keyframe_effect_impl);
  PushNewKeyframeModelsToImplThread(keyframe_effect_impl);

  // Now that the keyframe model lists are synchronized, push the properties for
  // the individual KeyframeModels.
  for (const auto& keyframe_model : keyframe_models()) {
    KeyframeModel* current_impl = KeyframeModel::ToCcKeyframeModel(
        keyframe_effect_impl->GetKeyframeModelById(keyframe_model->id()));
    if (current_impl)
      KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
          ->PushPropertiesTo(current_impl);
  }

  keyframe_effect_impl->UpdateTickingState();
}

std::string KeyframeEffect::KeyframeModelsToString() const {
  std::string str;
  for (size_t i = 0; i < keyframe_models().size(); i++) {
    if (i > 0)
      str.append(", ");
    str.append(KeyframeModel::ToCcKeyframeModel(keyframe_models()[i].get())
                   ->ToString());
  }
  return str;
}

base::TimeDelta KeyframeEffect::MinimumTickInterval() const {
  base::TimeDelta min_interval = base::TimeDelta::Max();
  for (const auto& model : keyframe_models()) {
    base::TimeDelta interval = model->curve()->TickInterval();
    if (interval.is_zero())
      return interval;
    if (interval < min_interval)
      min_interval = interval;
  }
  return min_interval;
}

void KeyframeEffect::RemoveKeyframeModelRange(
    typename KeyframeModels::iterator to_remove_begin,
    typename KeyframeModels::iterator to_remove_end) {
  bool keyframe_model_removed = false;
  for (auto it = to_remove_begin; it != to_remove_end; ++it) {
    if ((*it)->TargetProperty() == TargetProperty::SCROLL_OFFSET) {
      if (has_bound_element_animations())
        scroll_offset_animation_was_interrupted_ = true;
    } else if (!(*it)->is_finished()) {
      keyframe_model_removed = true;
    }
  }

  gfx::KeyframeEffect::RemoveKeyframeModelRange(to_remove_begin, to_remove_end);

  if (has_bound_element_animations()) {
    UpdateTickingState();
    if (keyframe_model_removed)
      element_animations_->UpdateClientAnimationState();
    animation_->SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::StartKeyframeModels(base::TimeTicks monotonic_time) {
  DCHECK(needs_to_start_keyframe_models_);
  needs_to_start_keyframe_models_ = false;

  // First collect running properties affecting each type of element.
  gfx::TargetProperties blocked_properties_for_active_elements;
  gfx::TargetProperties blocked_properties_for_pending_elements;
  std::vector<size_t> keyframe_models_waiting_for_target;

  keyframe_models_waiting_for_target.reserve(keyframe_models().size());
  for (size_t i = 0; i < keyframe_models().size(); ++i) {
    auto* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_models()[i].get());
    if (cc_keyframe_model->run_state() == gfx::KeyframeModel::STARTING ||
        cc_keyframe_model->run_state() == gfx::KeyframeModel::RUNNING) {
      int property = cc_keyframe_model->TargetProperty();
      if (cc_keyframe_model->affects_active_elements()) {
        blocked_properties_for_active_elements[property] = true;
      }
      if (cc_keyframe_model->affects_pending_elements()) {
        blocked_properties_for_pending_elements[property] = true;
      }
    } else if (cc_keyframe_model->run_state() ==
               gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY) {
      keyframe_models_waiting_for_target.push_back(i);
    }
  }

  for (size_t i = 0; i < keyframe_models_waiting_for_target.size(); ++i) {
    // Collect all properties for KeyframeModels with the same group id (they
    // should all also be in the list of KeyframeModels).
    size_t keyframe_model_index = keyframe_models_waiting_for_target[i];
    KeyframeModel* keyframe_model_waiting_for_target =
        KeyframeModel::ToCcKeyframeModel(
            keyframe_models()[keyframe_model_index].get());
    // Check for the run state again even though the keyframe_model was waiting
    // for target because it might have changed the run state while handling
    // previous keyframe_model in this loop (if they belong to same group).
    if (keyframe_model_waiting_for_target->run_state() ==
        gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY) {
      gfx::TargetProperties enqueued_properties;
      bool affects_active_elements =
          keyframe_model_waiting_for_target->affects_active_elements();
      bool affects_pending_elements =
          keyframe_model_waiting_for_target->affects_pending_elements();
      enqueued_properties[keyframe_model_waiting_for_target->TargetProperty()] =
          true;
      for (size_t j = keyframe_model_index + 1; j < keyframe_models().size();
           ++j) {
        auto* cc_keyframe_model =
            KeyframeModel::ToCcKeyframeModel(keyframe_models()[j].get());
        if (keyframe_model_waiting_for_target->group() ==
            cc_keyframe_model->group()) {
          enqueued_properties[cc_keyframe_model->TargetProperty()] = true;
          affects_active_elements |=
              cc_keyframe_model->affects_active_elements();
          affects_pending_elements |=
              cc_keyframe_model->affects_pending_elements();
        }
      }

      // Check to see if intersection of the list of properties affected by
      // the group and the list of currently blocked properties is null, taking
      // into account the type(s) of elements affected by the group. In any
      // case, the group's target properties need to be added to the lists of
      // blocked properties.
      bool null_intersection = true;
      for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
           property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
        if (enqueued_properties[property]) {
          if (affects_active_elements) {
            if (blocked_properties_for_active_elements[property])
              null_intersection = false;
            else
              blocked_properties_for_active_elements[property] = true;
          }
          if (affects_pending_elements) {
            if (blocked_properties_for_pending_elements[property])
              null_intersection = false;
            else
              blocked_properties_for_pending_elements[property] = true;
          }
        }
      }

      // If the intersection is null, then we are free to start the
      // KeyframeModels in the group.
      if (null_intersection) {
        keyframe_model_waiting_for_target->SetRunState(
            gfx::KeyframeModel::STARTING, monotonic_time);
        for (size_t j = keyframe_model_index + 1; j < keyframe_models().size();
             ++j) {
          auto* cc_keyframe_model =
              KeyframeModel::ToCcKeyframeModel(keyframe_models()[j].get());
          if (keyframe_model_waiting_for_target->group() ==
              cc_keyframe_model->group()) {
            cc_keyframe_model->SetRunState(gfx::KeyframeModel::STARTING,
                                           monotonic_time);
          }
        }
      } else {
        needs_to_start_keyframe_models_ = true;
      }
    }
  }
}

void KeyframeEffect::PromoteStartedKeyframeModels(AnimationEvents* events) {
  for (auto& keyframe_model : keyframe_models()) {
    if (keyframe_model->run_state() == gfx::KeyframeModel::STARTING &&
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get())
            ->affects_active_elements()) {
      auto* cc_keyframe_model =
          KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
      cc_keyframe_model->SetRunState(
          gfx::KeyframeModel::RUNNING,
          last_tick_time_.value_or(base::TimeTicks()));
      if (!cc_keyframe_model->has_set_start_time() &&
          !cc_keyframe_model->needs_synchronized_start_time())
        cc_keyframe_model->set_start_time(
            last_tick_time_.value_or(base::TimeTicks()));

      base::TimeTicks start_time;
      if (cc_keyframe_model->has_set_start_time())
        start_time = cc_keyframe_model->start_time();
      else
        start_time = last_tick_time_.value_or(base::TimeTicks());

      GenerateEvent(events, *cc_keyframe_model, AnimationEvent::Type::kStarted,
                    start_time);
    }
  }
}

void KeyframeEffect::MarkKeyframeModelsForDeletion(
    base::TimeTicks monotonic_time,
    AnimationEvents* events) {
  bool marked_keyframe_model_for_deletion = false;
  auto MarkForDeletion = [&](KeyframeModel* keyframe_model) {
    keyframe_model->SetRunState(gfx::KeyframeModel::WAITING_FOR_DELETION,
                                monotonic_time);
    marked_keyframe_model_for_deletion = true;
  };

  // Non-aborted KeyframeModels are marked for deletion after a corresponding
  // AnimationEvent::Type::kFinished event is sent or received. This means that
  // if we don't have an events vector, we must ensure that non-aborted
  // KeyframeModels have received a finished event before marking them for
  // deletion.
  for (auto& keyframe_model : keyframe_models()) {
    KeyframeModel* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
    if (cc_keyframe_model->run_state() == gfx::KeyframeModel::ABORTED) {
      GenerateEvent(events, *cc_keyframe_model, AnimationEvent::Type::kAborted,
                    monotonic_time);
      // If this is the controlling instance or it has already received finish
      // event, keyframe model can be marked for deletion.
      if (!NeedsFinishedEvent(cc_keyframe_model))
        MarkForDeletion(cc_keyframe_model);
      continue;
    }

    // If this is an aborted controlling instance that need completion on the
    // main thread, generate takeover event.
    if (cc_keyframe_model->is_controlling_instance() &&
        cc_keyframe_model->run_state() ==
            gfx::KeyframeModel::ABORTED_BUT_NEEDS_COMPLETION) {
      GenerateTakeoverEventForScrollAnimation(events, *cc_keyframe_model,
                                              monotonic_time);
      // Remove the keyframe model from the impl thread.
      MarkForDeletion(cc_keyframe_model);
      continue;
    }

    if (cc_keyframe_model->run_state() != gfx::KeyframeModel::FINISHED)
      continue;

    // Since deleting an animation on the main thread leads to its deletion
    // on the impl thread, we only mark a FINISHED main thread animation for
    // deletion once it has received a FINISHED event from the impl thread.
    if (NeedsFinishedEvent(cc_keyframe_model))
      continue;

    // If a keyframe model is finished, and not already marked for deletion,
    // find out if all other keyframe models in the same group are also
    // finished.
    std::vector<size_t> keyframe_models_in_same_group =
        FindAnimationsWithSameGroupId(keyframe_models(),
                                      cc_keyframe_model->group());

    bool a_keyframe_model_in_same_group_is_not_finished =
        base::ranges::any_of(keyframe_models_in_same_group, [&](size_t index) {
          auto* keyframe_model =
              KeyframeModel::ToCcKeyframeModel(keyframe_models()[index].get());
          return !keyframe_model->is_finished() ||
                 (keyframe_model->run_state() == gfx::KeyframeModel::FINISHED &&
                  NeedsFinishedEvent(keyframe_model));
        });

    if (a_keyframe_model_in_same_group_is_not_finished)
      continue;

    // Now remove all the keyframe models which belong to the same group and are
    // not yet aborted. These will be set to WAITING_FOR_DELETION which also
    // ensures we don't try to delete them again.
    for (size_t j = 0; j < keyframe_models_in_same_group.size(); ++j) {
      KeyframeModel* same_group_keyframe_model =
          KeyframeModel::ToCcKeyframeModel(
              keyframe_models()[keyframe_models_in_same_group[j]].get());

      // Skip any keyframe model in this group which is already processed.
      if (same_group_keyframe_model->run_state() ==
              gfx::KeyframeModel::WAITING_FOR_DELETION ||
          same_group_keyframe_model->run_state() == gfx::KeyframeModel::ABORTED)
        continue;

      GenerateEvent(events, *same_group_keyframe_model,
                    AnimationEvent::Type::kFinished, monotonic_time);
      MarkForDeletion(same_group_keyframe_model);
    }
  }

  // We need to purge KeyframeModels marked for deletion, which happens in
  // PushPropertiesTo().
  if (marked_keyframe_model_for_deletion)
    SetNeedsPushProperties();
}

void KeyframeEffect::MarkFinishedKeyframeModels(
    base::TimeTicks monotonic_time) {
  DCHECK(has_bound_element_animations());

  bool keyframe_model_finished = false;
  for (auto& keyframe_model : keyframe_models()) {
    // Scroll driven animations are never finished as the user may scroll back
    // into the active range.
    if (!animation_->IsScrollLinkedAnimation() &&
        !keyframe_model->is_finished() &&
        keyframe_model->IsFinishedAt(monotonic_time)) {
      keyframe_model->SetRunState(gfx::KeyframeModel::FINISHED, monotonic_time);
      keyframe_model_finished = true;
      SetNeedsPushProperties();
    }
    auto* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get());

    if (!cc_keyframe_model->affects_active_elements() &&
        !cc_keyframe_model->affects_pending_elements()) {
      switch (keyframe_model->run_state()) {
        case gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY:
        case gfx::KeyframeModel::STARTING:
        case gfx::KeyframeModel::RUNNING:
        case gfx::KeyframeModel::PAUSED:
          keyframe_model->SetRunState(gfx::KeyframeModel::FINISHED,
                                      monotonic_time);
          keyframe_model_finished = true;
          break;
        default:
          break;
      }
    }
  }
  if (keyframe_model_finished)
    element_animations_->UpdateClientAnimationState();
}

std::optional<gfx::PointF> KeyframeEffect::ScrollOffsetForAnimation() const {
  return element_animations_->ScrollOffsetForAnimation();
}

void KeyframeEffect::GenerateEvent(AnimationEvents* events,
                                   const KeyframeModel& keyframe_model,
                                   AnimationEvent::Type type,
                                   base::TimeTicks monotonic_time) {
  // An ungrouped model has been replaced by another model so avoid dispatching
  // any events from it.
  if (!events || keyframe_model.group() == KeyframeModel::kInvalidGroup) {
    return;
  }

  AnimationEvent event(type,
                       {animation_->animation_timeline()->id(),
                        animation_->id(), keyframe_model.id()},
                       keyframe_model.group(), keyframe_model.TargetProperty(),
                       monotonic_time);
  event.is_impl_only =
      KeyframeModel::ToCcKeyframeModel(&keyframe_model)->is_impl_only();
  if (!event.is_impl_only) {
    events->events_.push_back(event);
    return;
  }
  // For impl only animations notify delegate directly, do not record the event.
  animation_->DispatchAndDelegateAnimationEvent(event);
}

void KeyframeEffect::GenerateTakeoverEventForScrollAnimation(
    AnimationEvents* events,
    const KeyframeModel& keyframe_model,
    base::TimeTicks monotonic_time) {
  DCHECK_EQ(keyframe_model.TargetProperty(), TargetProperty::SCROLL_OFFSET);
  if (!events)
    return;

  AnimationEvent takeover_event(
      AnimationEvent::Type::kTakeOver,
      {animation_->animation_timeline()->id(), animation_->id(),
       keyframe_model.id()},
      keyframe_model.group(), keyframe_model.TargetProperty(), monotonic_time);
  takeover_event.animation_start_time = keyframe_model.start_time();
  const ScrollOffsetAnimationCurve* scroll_offset_animation_curve =
      ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
          keyframe_model.curve());
  takeover_event.curve = scroll_offset_animation_curve->Clone();
  // Notify main thread.
  events->events_.push_back(takeover_event);

  AnimationEvent finished_event(
      AnimationEvent::Type::kFinished,
      {animation_->animation_timeline()->id(), animation_->id(),
       keyframe_model.id()},
      keyframe_model.group(), keyframe_model.TargetProperty(), monotonic_time);
  // Notify the compositor that the animation is finished.
  finished_event.is_impl_only = true;
  animation_->DispatchAndDelegateAnimationEvent(finished_event);
}

}  // namespace cc
