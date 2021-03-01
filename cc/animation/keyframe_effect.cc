// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframe_effect.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/trees/property_animation_state.h"
#include "ui/gfx//transform_operations.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/target_property.h"

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
    const std::vector<std::unique_ptr<KeyframeModel>>& keyframe_models,
    int group_id) {
  std::vector<size_t> group;
  for (size_t i = 0; i < keyframe_models.size(); ++i) {
    if (keyframe_models[i]->group() != group_id)
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
      needs_push_properties_(false) {}

KeyframeEffect::~KeyframeEffect() {
  DCHECK(!has_bound_element_animations());
}

void KeyframeEffect::SetNeedsPushProperties() {
  needs_push_properties_ = true;

  // TODO(smcgruer): We only need the below calls when needs_push_properties_
  // goes from false to true - see http://crbug.com/764405
  DCHECK(element_animations());
  element_animations()->SetNeedsPushProperties();

  animation_->SetNeedsPushProperties();
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

void KeyframeEffect::Tick(base::TimeTicks monotonic_time) {
  DCHECK(has_bound_element_animations());
  if (!element_animations_->has_element_in_any_list())
    return;

  if (needs_to_start_keyframe_models_)
    StartKeyframeModels(monotonic_time);

  for (auto& keyframe_model : keyframe_models_) {
    TickKeyframeModel(monotonic_time, keyframe_model.get());
  }

  last_tick_time_ = monotonic_time;
  element_animations_->UpdateClientAnimationState();
}

void KeyframeEffect::TickKeyframeModel(base::TimeTicks monotonic_time,
                                       KeyframeModel* keyframe_model) {
  if ((keyframe_model->run_state() != gfx::KeyframeModel::STARTING &&
       keyframe_model->run_state() != gfx::KeyframeModel::RUNNING &&
       keyframe_model->run_state() != gfx::KeyframeModel::PAUSED) ||
      !keyframe_model->InEffect(monotonic_time)) {
    return;
  }

  gfx::AnimationCurve* curve = keyframe_model->curve();
  base::TimeDelta trimmed =
      keyframe_model->TrimTimeToCurrentIteration(monotonic_time);
  curve->Tick(trimmed, keyframe_model->TargetProperty(), keyframe_model);
}

void KeyframeEffect::RemoveFromTicking() {
  is_ticking_ = false;
  // Resetting last_tick_time_ here ensures that calling ::UpdateState
  // before ::Animate doesn't start a keyframe model.
  last_tick_time_ = base::nullopt;
  animation_->RemoveFromTicking();
}

void KeyframeEffect::UpdateState(bool start_ready_keyframe_models,
                                 AnimationEvents* events) {
  DCHECK(has_bound_element_animations());

  // Animate hasn't been called, this happens if an element has been added
  // between the Commit and Draw phases.
  if (last_tick_time_ == base::nullopt)
    start_ready_keyframe_models = false;

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

  if (!element_animations()->has_element_in_any_list())
    RemoveFromTicking();
}

void KeyframeEffect::UpdateTickingState() {
  if (animation_->has_animation_host()) {
    bool was_ticking = is_ticking_;
    is_ticking_ = HasNonDeletedKeyframeModel() &&
                  element_animations_->has_element_in_any_list();

    if (is_ticking_ && !was_ticking) {
      animation_->AddToTicking();
    } else if (!is_ticking_ && was_ticking) {
      RemoveFromTicking();
    }
  }
}

void KeyframeEffect::Pause(base::TimeDelta pause_offset,
                           PauseCondition pause_condition) {
  bool did_pause = false;
  for (auto& keyframe_model : keyframe_models_) {
    // TODO(crbug.com/1076012): KeyframeEffect is paused with local time for
    // scroll-linked animations. To make sure the start event of a keyframe
    // model is sent to blink, we should not set its run state to PAUSED until
    // such event is sent. This should be revisited once KeyframeEffect is able
    // to tick scroll-linked keyframe models directly.
    if (pause_condition == PauseCondition::kAfterStart &&
        (keyframe_model->run_state() ==
             gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY ||
         keyframe_model->run_state() == gfx::KeyframeModel::STARTING))
      continue;
    keyframe_model->Pause(pause_offset);
    did_pause = true;
  }

  if (!did_pause || !has_bound_element_animations())
    return;
  animation_->SetNeedsCommit();
  SetNeedsPushProperties();
}

void KeyframeEffect::AddKeyframeModel(
    std::unique_ptr<KeyframeModel> keyframe_model) {
  DCHECK(!keyframe_model->is_impl_only() ||
         keyframe_model->TargetProperty() == TargetProperty::SCROLL_OFFSET);
  // This is to make sure that keyframe models in the same group, i.e., start
  // together, don't animate the same property.
  DCHECK(std::none_of(keyframe_models_.begin(), keyframe_models_.end(),
                      [&](const auto& existing_keyframe_model) {
                        return keyframe_model->TargetProperty() ==
                                   existing_keyframe_model->TargetProperty() &&
                               keyframe_model->group() ==
                                   existing_keyframe_model->group();
                      }));

  if (keyframe_model->TargetProperty() == TargetProperty::SCROLL_OFFSET) {
    // We should never have more than one scroll offset animation queued on the
    // same scrolling element as this would result in multiple automated
    // scrolls.
    DCHECK(std::none_of(
        keyframe_models_.begin(), keyframe_models_.end(),
        [&](const auto& existing_keyframe_model) {
          return existing_keyframe_model->TargetProperty() ==
                     TargetProperty::SCROLL_OFFSET &&
                 !existing_keyframe_model->is_finished() &&
                 (!existing_keyframe_model->is_controlling_instance() ||
                  existing_keyframe_model->affects_pending_elements());
        }));
  }

  keyframe_models_.push_back(std::move(keyframe_model));

  if (has_bound_element_animations()) {
    KeyframeModelAdded();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::PauseKeyframeModel(int keyframe_model_id,
                                        base::TimeDelta time_offset) {
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->id() == keyframe_model_id) {
      keyframe_model->Pause(time_offset);
    }
  }

  if (has_bound_element_animations()) {
    animation_->SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::RemoveKeyframeModel(int keyframe_model_id) {
  bool keyframe_model_removed = false;

  // Since we want to use the KeyframeModels that we're going to remove, we
  // need to use a stable_partition here instead of remove_if. remove_if leaves
  // the removed items in an unspecified state.
  auto keyframe_models_to_remove = std::stable_partition(
      keyframe_models_.begin(), keyframe_models_.end(),
      [keyframe_model_id](
          const std::unique_ptr<KeyframeModel>& keyframe_model) {
        return keyframe_model->id() != keyframe_model_id;
      });
  for (auto it = keyframe_models_to_remove; it != keyframe_models_.end();
       ++it) {
    if ((*it)->TargetProperty() == TargetProperty::SCROLL_OFFSET) {
      if (has_bound_element_animations())
        scroll_offset_animation_was_interrupted_ = true;
    } else if (!(*it)->is_finished()) {
      keyframe_model_removed = true;
    }
  }

  keyframe_models_.erase(keyframe_models_to_remove, keyframe_models_.end());

  if (has_bound_element_animations()) {
    UpdateTickingState();
    if (keyframe_model_removed)
      element_animations_->UpdateClientAnimationState();
    animation_->SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void KeyframeEffect::AbortKeyframeModel(int keyframe_model_id) {
  if (KeyframeModel* keyframe_model = GetKeyframeModelById(keyframe_model_id)) {
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
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->TargetProperty() == target_property &&
        !keyframe_model->is_finished()) {
      // Currently only impl-only scroll offset KeyframeModels can be completed
      // on the main thread.
      if (needs_completion && keyframe_model->is_impl_only()) {
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
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->affects_active_elements() !=
        keyframe_model->affects_pending_elements()) {
      keyframe_model_activated = true;
    }
    keyframe_model->set_affects_active_elements(
        keyframe_model->affects_pending_elements());
  }

  if (keyframe_model_activated)
    element_animations_->UpdateClientAnimationState();

  scroll_offset_animation_was_interrupted_ = false;
}

void KeyframeEffect::KeyframeModelAdded() {
  DCHECK(has_bound_element_animations());

  animation_->SetNeedsCommit();
  needs_to_start_keyframe_models_ = true;

  UpdateTickingState();
  for (auto& keyframe_model : keyframe_models_) {
    element_animations_->AttachToCurve(keyframe_model->curve());
  }
  element_animations_->UpdateClientAnimationState();
}

bool KeyframeEffect::DispatchAnimationEventToKeyframeModel(
    const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  KeyframeModel* keyframe_model = GetKeyframeModelById(event.uid.model_id);
  bool dispatched = false;
  switch (event.type) {
    case AnimationEvent::STARTED:
      if (keyframe_model && keyframe_model->needs_synchronized_start_time()) {
        keyframe_model->set_needs_synchronized_start_time(false);
        if (!keyframe_model->has_set_start_time())
          keyframe_model->set_start_time(event.monotonic_time);
        dispatched = true;
      }
      break;

    case AnimationEvent::FINISHED:
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

    case AnimationEvent::ABORTED:
      if (keyframe_model) {
        keyframe_model->SetRunState(gfx::KeyframeModel::ABORTED,
                                    event.monotonic_time);
        keyframe_model->set_received_finished_event(true);
        dispatched = true;

        ElementAnimations* element_animations =
            animation_->animation_host()
                ->GetElementAnimationsForElementId(element_id())
                .get();
        if (element_animations)
          element_animations->UpdateClientAnimationState();
      }
      break;

    case AnimationEvent::TAKEOVER:
      // TODO(crbug.com/1018213): Routing TAKEOVER events is broken.
      // We need to purge KeyframeModels marked for deletion on CT.
      SetNeedsPushProperties();
      dispatched = true;
      break;

    case AnimationEvent::TIME_UPDATED:
      // TIME_UPDATED events are used to synchronize effect time between cc and
      // main thread worklet animations. Keyframe models are not involved in
      // this process.
      NOTREACHED();
      break;
  }
  return dispatched;
}

bool KeyframeEffect::HasTickingKeyframeModel() const {
  for (const auto& keyframe_model : keyframe_models_) {
    if (!keyframe_model->is_finished())
      return true;
  }
  return false;
}

bool KeyframeEffect::AffectsCustomProperty() const {
  for (const auto& it : keyframe_models_)
    if (it->TargetProperty() == TargetProperty::CSS_CUSTOM_PROPERTY)
      return true;
  return false;
}

bool KeyframeEffect::HasNonDeletedKeyframeModel() const {
  for (const auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->run_state() != gfx::KeyframeModel::WAITING_FOR_DELETION)
      return true;
  }
  return false;
}

bool KeyframeEffect::AnimationsPreserveAxisAlignment() const {
  for (const auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->is_finished())
      continue;

    if (!keyframe_model->curve()->PreservesAxisAlignment())
      return false;
  }
  return true;
}

float KeyframeEffect::MaximumScale(ElementListType list_type) const {
  float maximum_scale = kInvalidScale;
  for (const auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->is_finished())
      continue;

    if ((list_type == ElementListType::ACTIVE &&
         !keyframe_model->affects_active_elements()) ||
        (list_type == ElementListType::PENDING &&
         !keyframe_model->affects_pending_elements()))
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
  for (const auto& keyframe_model : keyframe_models_) {
    if (!keyframe_model->is_finished() &&
        keyframe_model->TargetProperty() == target_property) {
      if ((list_type == ElementListType::ACTIVE &&
           keyframe_model->affects_active_elements()) ||
          (list_type == ElementListType::PENDING &&
           keyframe_model->affects_pending_elements()))
        return true;
    }
  }
  return false;
}

bool KeyframeEffect::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (const auto& keyframe_model : keyframe_models_) {
    if (!keyframe_model->is_finished() &&
        keyframe_model->InEffect(last_tick_time_.value_or(base::TimeTicks())) &&
        keyframe_model->TargetProperty() == target_property) {
      if ((list_type == ElementListType::ACTIVE &&
           keyframe_model->affects_active_elements()) ||
          (list_type == ElementListType::PENDING &&
           keyframe_model->affects_pending_elements()))
        return true;
    }
  }
  return false;
}

KeyframeModel* KeyframeEffect::GetKeyframeModel(
    TargetProperty::Type target_property) const {
  for (size_t i = 0; i < keyframe_models_.size(); ++i) {
    size_t index = keyframe_models_.size() - i - 1;
    if (keyframe_models_[index]->TargetProperty() == target_property)
      return keyframe_models_[index].get();
  }
  return nullptr;
}

KeyframeModel* KeyframeEffect::GetKeyframeModelById(
    int keyframe_model_id) const {
  for (auto& keyframe_model : keyframe_models_)
    if (keyframe_model->id() == keyframe_model_id)
      return keyframe_model.get();
  return nullptr;
}

void KeyframeEffect::GetPropertyAnimationState(
    PropertyAnimationState* pending_state,
    PropertyAnimationState* active_state) const {
  pending_state->Clear();
  active_state->Clear();

  for (const auto& keyframe_model : keyframe_models_) {
    if (!keyframe_model->is_finished()) {
      bool in_effect =
          keyframe_model->InEffect(last_tick_time_.value_or(base::TimeTicks()));
      bool active = keyframe_model->affects_active_elements();
      bool pending = keyframe_model->affects_pending_elements();
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

  auto& keyframe_models_impl = keyframe_effect_impl->keyframe_models_;
  for (const auto& keyframe_model_impl : keyframe_models_impl) {
    // If the keyframe_model has been aborted on the main thread, mark it for
    // deletion.
    if (KeyframeModel* keyframe_model =
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
  base::EraseIf(
      keyframe_models_,
      [impl_only](const std::unique_ptr<KeyframeModel>& keyframe_model) {
        return keyframe_model->run_state() ==
                   gfx::KeyframeModel::WAITING_FOR_DELETION &&
               (!impl_only || keyframe_model->is_impl_only());
      });
}

void KeyframeEffect::PurgeDeletedKeyframeModels() {
  base::EraseIf(keyframe_models_,
                [](const std::unique_ptr<KeyframeModel>& keyframe_model) {
                  return keyframe_model->run_state() ==
                             gfx::KeyframeModel::WAITING_FOR_DELETION &&
                         !keyframe_model->affects_pending_elements();
                });
}

void KeyframeEffect::PushNewKeyframeModelsToImplThread(
    KeyframeEffect* keyframe_effect_impl) const {
  // Any new KeyframeModels owned by the main thread's Animation are
  // cloned and added to the impl thread's Animation.
  for (const auto& keyframe_model : keyframe_models_) {
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
      gfx::ScrollOffset current_scroll_offset;
      if (keyframe_effect_impl->HasElementInActiveList()) {
        current_scroll_offset =
            keyframe_effect_impl->ScrollOffsetForAnimation();
      } else {
        // The owning layer isn't yet in the active tree, so the main thread
        // scroll offset will be up to date.
        current_scroll_offset = ScrollOffsetForAnimation();
      }
      ScrollOffsetAnimationCurve* curve =
          ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
              keyframe_model->curve());
      curve->SetInitialValue(current_scroll_offset);
    }

    // The new keyframe_model should be set to run as soon as possible.
    gfx::KeyframeModel::RunState initial_run_state =
        gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY;
    std::unique_ptr<KeyframeModel> to_add(
        keyframe_model->CreateImplInstance(initial_run_state));
    DCHECK(!to_add->needs_synchronized_start_time());
    to_add->set_affects_active_elements(false);
    keyframe_effect_impl->AddKeyframeModel(std::move(to_add));
  }
}

namespace {
bool IsCompleted(KeyframeModel* keyframe_model,
                 const KeyframeEffect* main_thread_keyframe_effect) {
  if (keyframe_model->is_impl_only()) {
    return (keyframe_model->run_state() ==
            gfx::KeyframeModel::WAITING_FOR_DELETION);
  } else {
    KeyframeModel* main_thread_keyframe_model =
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
  for (const std::unique_ptr<KeyframeModel>& keyframe_model :
       keyframe_effect_impl->keyframe_models_) {
    if (IsCompleted(keyframe_model.get(), this)) {
      keyframe_model->set_affects_pending_elements(false);
      keyframe_model_completed = true;
    }
  }

  keyframe_effect_impl->PurgeDeletedKeyframeModels();
  if (has_bound_element_animations() && keyframe_model_completed)
    element_animations_->SetNeedsPushProperties();
}

void KeyframeEffect::PushPropertiesTo(KeyframeEffect* keyframe_effect_impl) {
  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  // Synchronize the keyframe_model target between main and impl side.
  if (element_id_ != keyframe_effect_impl->element_id_) {
    // We have to detach/attach via the Animation as it may need to inform
    // the host as well.
    if (keyframe_effect_impl->has_attached_element())
      keyframe_effect_impl->animation_->DetachElement();
    if (element_id_) {
      if (element_id_.GetStableId() == ElementId::kReservedElementId)
        keyframe_effect_impl->animation_->AttachNoElement();
      else
        keyframe_effect_impl->animation_->AttachElement(element_id_);
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

  // Synchronize the main-thread and impl-side keyframe model lists, removing
  // aborted KeyframeModels and pushing any new animations.
  MarkAbortedKeyframeModelsForDeletion(keyframe_effect_impl);
  PurgeKeyframeModelsMarkedForDeletion(/* impl_only */ false);
  RemoveKeyframeModelsCompletedOnMainThread(keyframe_effect_impl);
  PushNewKeyframeModelsToImplThread(keyframe_effect_impl);

  // Now that the keyframe model lists are synchronized, push the properties for
  // the individual KeyframeModels.
  for (const auto& keyframe_model : keyframe_models_) {
    KeyframeModel* current_impl =
        keyframe_effect_impl->GetKeyframeModelById(keyframe_model->id());
    if (current_impl)
      keyframe_model->PushPropertiesTo(current_impl);
  }

  keyframe_effect_impl->UpdateTickingState();
}

std::string KeyframeEffect::KeyframeModelsToString() const {
  std::string str;
  for (size_t i = 0; i < keyframe_models_.size(); i++) {
    if (i > 0)
      str.append(", ");
    str.append(keyframe_models_[i]->ToString());
  }
  return str;
}

base::TimeDelta KeyframeEffect::MinimumTickInterval() const {
  base::TimeDelta min_interval = base::TimeDelta::Max();
  for (const auto& model : keyframe_models_) {
    base::TimeDelta interval = model->curve()->TickInterval();
    if (interval.is_zero())
      return interval;
    if (interval < min_interval)
      min_interval = interval;
  }
  return min_interval;
}

void KeyframeEffect::StartKeyframeModels(base::TimeTicks monotonic_time) {
  DCHECK(needs_to_start_keyframe_models_);
  needs_to_start_keyframe_models_ = false;

  // First collect running properties affecting each type of element.
  gfx::TargetProperties blocked_properties_for_active_elements;
  gfx::TargetProperties blocked_properties_for_pending_elements;
  std::vector<size_t> keyframe_models_waiting_for_target;

  keyframe_models_waiting_for_target.reserve(keyframe_models_.size());
  for (size_t i = 0; i < keyframe_models_.size(); ++i) {
    auto& keyframe_model = keyframe_models_[i];
    if (keyframe_model->run_state() == gfx::KeyframeModel::STARTING ||
        keyframe_model->run_state() == gfx::KeyframeModel::RUNNING) {
      int property = keyframe_model->TargetProperty();
      if (keyframe_model->affects_active_elements()) {
        blocked_properties_for_active_elements[property] = true;
      }
      if (keyframe_model->affects_pending_elements()) {
        blocked_properties_for_pending_elements[property] = true;
      }
    } else if (keyframe_model->run_state() ==
               gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY) {
      keyframe_models_waiting_for_target.push_back(i);
    }
  }

  for (size_t i = 0; i < keyframe_models_waiting_for_target.size(); ++i) {
    // Collect all properties for KeyframeModels with the same group id (they
    // should all also be in the list of KeyframeModels).
    size_t keyframe_model_index = keyframe_models_waiting_for_target[i];
    KeyframeModel* keyframe_model_waiting_for_target =
        keyframe_models_[keyframe_model_index].get();
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
      for (size_t j = keyframe_model_index + 1; j < keyframe_models_.size();
           ++j) {
        if (keyframe_model_waiting_for_target->group() ==
            keyframe_models_[j]->group()) {
          enqueued_properties[keyframe_models_[j]->TargetProperty()] = true;
          affects_active_elements |=
              keyframe_models_[j]->affects_active_elements();
          affects_pending_elements |=
              keyframe_models_[j]->affects_pending_elements();
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
        for (size_t j = keyframe_model_index + 1; j < keyframe_models_.size();
             ++j) {
          if (keyframe_model_waiting_for_target->group() ==
              keyframe_models_[j]->group()) {
            keyframe_models_[j]->SetRunState(gfx::KeyframeModel::STARTING,
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
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->run_state() == gfx::KeyframeModel::STARTING &&
        keyframe_model->affects_active_elements()) {
      keyframe_model->SetRunState(gfx::KeyframeModel::RUNNING,
                                  last_tick_time_.value_or(base::TimeTicks()));
      if (!keyframe_model->has_set_start_time() &&
          !keyframe_model->needs_synchronized_start_time())
        keyframe_model->set_start_time(
            last_tick_time_.value_or(base::TimeTicks()));

      base::TimeTicks start_time;
      if (keyframe_model->has_set_start_time())
        start_time = keyframe_model->start_time();
      else
        start_time = last_tick_time_.value_or(base::TimeTicks());

      GenerateEvent(events, *keyframe_model, AnimationEvent::STARTED,
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
  // AnimationEvent::FINISHED event is sent or received. This means that if
  // we don't have an events vector, we must ensure that non-aborted
  // KeyframeModels have received a finished event before marking them for
  // deletion.
  for (size_t i = 0; i < keyframe_models_.size(); i++) {
    KeyframeModel* keyframe_model = keyframe_models_[i].get();
    if (keyframe_model->run_state() == gfx::KeyframeModel::ABORTED) {
      GenerateEvent(events, *keyframe_model, AnimationEvent::ABORTED,
                    monotonic_time);
      // If this is the controlling instance or it has already received finish
      // event, keyframe model can be marked for deletion.
      if (!NeedsFinishedEvent(keyframe_model))
        MarkForDeletion(keyframe_model);
      continue;
    }

    // If this is an aborted controlling instance that need completion on the
    // main thread, generate takeover event.
    if (keyframe_model->is_controlling_instance() &&
        keyframe_model->run_state() ==
            gfx::KeyframeModel::ABORTED_BUT_NEEDS_COMPLETION) {
      GenerateTakeoverEventForScrollAnimation(events, *keyframe_model,
                                              monotonic_time);
      // Remove the keyframe model from the impl thread.
      MarkForDeletion(keyframe_model);
      continue;
    }

    if (keyframe_model->run_state() != gfx::KeyframeModel::FINISHED)
      continue;

    // Since deleting an animation on the main thread leads to its deletion
    // on the impl thread, we only mark a FINISHED main thread animation for
    // deletion once it has received a FINISHED event from the impl thread.
    if (NeedsFinishedEvent(keyframe_model))
      continue;

    // If a keyframe model is finished, and not already marked for deletion,
    // find out if all other keyframe models in the same group are also
    // finished.
    std::vector<size_t> keyframe_models_in_same_group =
        FindAnimationsWithSameGroupId(keyframe_models_,
                                      keyframe_model->group());

    bool a_keyframe_model_in_same_group_is_not_finished = std::any_of(
        keyframe_models_in_same_group.cbegin(),
        keyframe_models_in_same_group.cend(), [&](size_t index) {
          KeyframeModel* keyframe_model = keyframe_models_[index].get();
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
          keyframe_models_[keyframe_models_in_same_group[j]].get();

      // Skip any keyframe model in this group which is already processed.
      if (same_group_keyframe_model->run_state() ==
              gfx::KeyframeModel::WAITING_FOR_DELETION ||
          same_group_keyframe_model->run_state() == gfx::KeyframeModel::ABORTED)
        continue;

      GenerateEvent(events, *same_group_keyframe_model,
                    AnimationEvent::FINISHED, monotonic_time);
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
  for (auto& keyframe_model : keyframe_models_) {
    if (!keyframe_model->is_finished() &&
        keyframe_model->IsFinishedAt(monotonic_time)) {
      keyframe_model->SetRunState(gfx::KeyframeModel::FINISHED, monotonic_time);
      keyframe_model_finished = true;
      SetNeedsPushProperties();
    }
    if (!keyframe_model->affects_active_elements() &&
        !keyframe_model->affects_pending_elements()) {
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

bool KeyframeEffect::HasElementInActiveList() const {
  DCHECK(has_bound_element_animations());
  return element_animations_->has_element_in_active_list();
}

gfx::ScrollOffset KeyframeEffect::ScrollOffsetForAnimation() const {
  DCHECK(has_bound_element_animations());
  return element_animations_->ScrollOffsetForAnimation();
}

void KeyframeEffect::GenerateEvent(AnimationEvents* events,
                                   const KeyframeModel& keyframe_model,
                                   AnimationEvent::Type type,
                                   base::TimeTicks monotonic_time) {
  if (!events)
    return;

  AnimationEvent event(type,
                       {animation_->animation_timeline()->id(),
                        animation_->id(), keyframe_model.id()},
                       keyframe_model.group(), keyframe_model.TargetProperty(),
                       monotonic_time);
  event.is_impl_only = keyframe_model.is_impl_only();
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
      AnimationEvent::TAKEOVER,
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
      AnimationEvent::FINISHED,
      {animation_->animation_timeline()->id(), animation_->id(),
       keyframe_model.id()},
      keyframe_model.group(), keyframe_model.TargetProperty(), monotonic_time);
  // Notify the compositor that the animation is finished.
  finished_event.is_impl_only = true;
  animation_->DispatchAndDelegateAnimationEvent(finished_event);
}

}  // namespace cc
