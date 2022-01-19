// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_animation_registry.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/system/holding_space/holding_space_progress_icon_animation.h"
#include "ash/system/holding_space/holding_space_progress_ring_animation.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns the owner for the singleton `HoldingSpaceAnimationRegistry` instance.
std::unique_ptr<HoldingSpaceAnimationRegistry>& GetInstanceOwner() {
  static base::NoDestructor<std::unique_ptr<HoldingSpaceAnimationRegistry>>
      instance_owner;
  return *instance_owner;
}

}  // namespace

// HoldingSpaceAnimationRegistry::ProgressIndicatorAnimationDelegate -----------

// The delegate of `HoldingSpaceAnimationRegistry` responsible for creating and
// curating progress indicator animations based on holding space model state.
class HoldingSpaceAnimationRegistry::ProgressIndicatorAnimationDelegate
    : public HoldingSpaceControllerObserver,
      public HoldingSpaceModelObserver {
 public:
  explicit ProgressIndicatorAnimationDelegate(
      HoldingSpaceController* controller)
      : controller_(controller) {
    controller_observation_.Observe(controller_);
    if (controller_->model())
      OnHoldingSpaceModelAttached(controller_->model());
  }

  ProgressIndicatorAnimationDelegate(
      const ProgressIndicatorAnimationDelegate&) = delete;
  ProgressIndicatorAnimationDelegate& operator=(
      const ProgressIndicatorAnimationDelegate&) = delete;
  ~ProgressIndicatorAnimationDelegate() override = default;

  // Adds the specified `callback` to be notified of changes to the icon
  // animation associated with the specified `key`. The `callback` will continue
  // to receive events so long as both `this` and the returned subscription
  // exist.
  base::CallbackListSubscription AddIconAnimationChangedCallbackForKey(
      const void* key,
      ProgressIconAnimationChangedCallbackList::CallbackType callback) {
    auto it = icon_animation_changed_callback_lists_by_key_.find(key);

    // If this is the first time that an icon animation changed callback is
    // being registered for the specified `key`, set a callback to destroy the
    // created callback list when it becomes empty.
    if (it == icon_animation_changed_callback_lists_by_key_.end()) {
      it = icon_animation_changed_callback_lists_by_key_
               .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                        std::forward_as_tuple())
               .first;
      it->second.set_removal_callback(base::BindRepeating(
          &ProgressIndicatorAnimationDelegate::
              EraseIconAnimationChangedCallbackListForKeyIfEmpty,
          base::Unretained(this), base::Unretained(key)));
    }

    return it->second.Add(std::move(callback));
  }

  // Adds the specified `callback` to be notified of changes to the ring
  // animation associated with the specified `key`. The `callback` will continue
  // to receive events so long as both `this` and the returned subscription
  // exist.
  base::CallbackListSubscription AddRingAnimationChangedCallbackForKey(
      const void* key,
      ProgressRingAnimationChangedCallbackList::CallbackType callback) {
    auto it = ring_animation_changed_callback_lists_by_key_.find(key);

    // If this is the first time that a ring animation changed callback is being
    // registered for the specified `key`, set a callback to destroy the
    // created callback list when it becomes empty.
    if (it == ring_animation_changed_callback_lists_by_key_.end()) {
      it = ring_animation_changed_callback_lists_by_key_
               .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                        std::forward_as_tuple())
               .first;
      it->second.set_removal_callback(base::BindRepeating(
          &ProgressIndicatorAnimationDelegate::
              EraseRingAnimationChangedCallbackListForKeyIfEmpty,
          base::Unretained(this), base::Unretained(key)));
    }

    return it->second.Add(std::move(callback));
  }

  // Returns the registered icon animation for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  HoldingSpaceProgressIconAnimation* GetIconAnimationForKey(const void* key) {
    auto it = icon_animations_by_key_.find(key);
    return it != icon_animations_by_key_.end() ? it->second.get() : nullptr;
  }

  // Returns the registered ring animation for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  HoldingSpaceProgressRingAnimation* GetRingAnimationForKey(const void* key) {
    auto it = ring_animations_by_key_.find(key);
    return it != ring_animations_by_key_.end() ? it->second.animation.get()
                                               : nullptr;
  }

 private:
  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override {
    model_ = model;
    model_observation_.Observe(model_);
    UpdateAnimations(/*for_removal=*/false);
  }

  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override {
    model_ = nullptr;
    model_observation_.Reset();
    UpdateAnimations(/*for_removal=*/false);
  }

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override {
    UpdateAnimations(/*for_removal=*/false);
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    // The removal of `items` can be safely ignored if none were in progress.
    const bool removed_in_progress_item = std::any_of(
        items.begin(), items.end(), [](const HoldingSpaceItem* item) {
          return item->IsInitialized() && !item->progress().IsComplete();
        });
    if (removed_in_progress_item)
      UpdateAnimations(/*for_removal=*/true);
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    UpdateAnimations(/*for_removal=*/false);
  }

  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override {
    // The `item` update can be safely ignored if progress has not been updated.
    if (!(updated_fields & HoldingSpaceModelObserver::UpdatedField::kProgress))
      return;

    // If `item` has just progressed to completion, ensure that a pulse
    // animation is created and started.
    if (item->progress().IsComplete()) {
      EnsureRingAnimationOfTypeForKey(
          item, HoldingSpaceProgressRingAnimation::Type::kPulse);
    }

    UpdateAnimations(/*for_removal=*/false);
  }

  // Erases all animations, notifying any animation changed callbacks.
  void EraseAllAnimations() {
    EraseAllAnimationsForKeyIf(
        base::BindRepeating([](const void* key) { return true; }));
  }

  // Erases all animations for the specified `key`, notifying any animation
  // changed callbacks.
  void EraseAllAnimationsForKey(const void* key) {
    EraseIconAnimationForKey(key);
    EraseRingAnimationForKey(key);
  }

  // Erases all animations for keys for which `predicate` returns `true`,
  // notifying any animation changed callbacks.
  void EraseAllAnimationsForKeyIf(
      base::RepeatingCallback<bool(const void* key)> predicate) {
    std::set<const void*> keys_to_erase;
    for (const auto& icon_animation_by_key : icon_animations_by_key_) {
      const void* key = icon_animation_by_key.first;
      if (predicate.Run(key))
        keys_to_erase.insert(key);
    }
    for (const auto& ring_animation_by_key : ring_animations_by_key_) {
      const void* key = ring_animation_by_key.first;
      if (predicate.Run(key))
        keys_to_erase.insert(key);
    }
    for (const void* key : keys_to_erase)
      EraseAllAnimationsForKey(key);
  }

  // Erases the icon animation callback list for the specified `key` if empty.
  void EraseIconAnimationChangedCallbackListForKeyIfEmpty(const void* key) {
    auto it = icon_animation_changed_callback_lists_by_key_.find(key);
    if (it == icon_animation_changed_callback_lists_by_key_.end())
      return;
    if (it->second.empty())
      icon_animation_changed_callback_lists_by_key_.erase(it);
  }

  // Erases the icon animation for the specified `key`, notifying any animation
  // changed callbacks.
  void EraseIconAnimationForKey(const void* key) {
    auto it = icon_animations_by_key_.find(key);
    if (it == icon_animations_by_key_.end())
      return;
    icon_animations_by_key_.erase(it);
    NotifyIconAnimationChangedForKey(key);
  }

  // Erases the ring animation for the specified `key`, notifying any animation
  // changed callbacks.
  void EraseRingAnimationForKey(const void* key) {
    auto it = ring_animations_by_key_.find(key);
    if (it == ring_animations_by_key_.end())
      return;
    ring_animations_by_key_.erase(it);
    NotifyRingAnimationChangedForKey(key);
  }

  // Erases the ring animation for the specified `key` if it is not of the
  // desired `type`, notifying any animation changed callbacks.
  void EraseRingAnimationIfNotOfTypeForKey(
      const void* key,
      HoldingSpaceProgressRingAnimation::Type type) {
    auto it = ring_animations_by_key_.find(key);
    if (it != ring_animations_by_key_.end() &&
        it->second.animation->type() != type) {
      EraseRingAnimationForKey(key);
    }
  }

  // Erases the ring animation callback list for the specified `key` if empty.
  void EraseRingAnimationChangedCallbackListForKeyIfEmpty(const void* key) {
    auto it = ring_animation_changed_callback_lists_by_key_.find(key);
    if (it == ring_animation_changed_callback_lists_by_key_.end())
      return;
    if (it->second.empty())
      ring_animation_changed_callback_lists_by_key_.erase(it);
  }

  // Ensures that the icon animation for the specified `key` exists. If
  // necessary, a new animation is created and started, notifying any animation
  // changed callbacks. NOTE: This method no-ops unless in-progress animations
  // v2 is enabled.
  void EnsureIconAnimationForKey(const void* key) {
    if (!features::IsHoldingSpaceInProgressAnimationV2Enabled())
      return;

    auto it = icon_animations_by_key_.find(key);
    if (it != icon_animations_by_key_.end())
      return;

    icon_animations_by_key_
        .emplace(key, std::make_unique<HoldingSpaceProgressIconAnimation>())
        .first->second->Start();

    NotifyIconAnimationChangedForKey(key);
  }

  // Ensures that the ring animation for the specified `key` is of the desired
  // `type`. If necessary, a new animation is created and started, notifying any
  // animation changed callbacks.
  void EnsureRingAnimationOfTypeForKey(
      const void* key,
      HoldingSpaceProgressRingAnimation::Type type) {
    auto it = ring_animations_by_key_.find(key);
    if (it != ring_animations_by_key_.end() &&
        it->second.animation->type() == type) {
      return;
    }

    auto animation = HoldingSpaceProgressRingAnimation::CreateOfType(type);

    auto subscription =
        animation->AddAnimationUpdatedCallback(base::BindRepeating(
            &ProgressIndicatorAnimationDelegate::OnRingAnimationUpdatedForKey,
            base::Unretained(this), key, animation.get()));

    auto subscribed_animation = SubscribedProgressRingAnimation{
        .animation = std::move(animation),
        .subscription = std::move(subscription)};

    if (it == ring_animations_by_key_.end()) {
      ring_animations_by_key_.emplace(key, std::move(subscribed_animation))
          .first->second.animation->Start();
    } else {
      it->second = std::move(subscribed_animation);
      it->second.animation->Start();
    }

    NotifyRingAnimationChangedForKey(key);
  }

  // Notifies any icon animation changed callbacks registered for the specified
  // `key` that the associated animation has changed.
  void NotifyIconAnimationChangedForKey(const void* key) {
    auto it = icon_animation_changed_callback_lists_by_key_.find(key);
    if (it == icon_animation_changed_callback_lists_by_key_.end())
      return;
    auto animation_it = icon_animations_by_key_.find(key);
    it->second.Notify(animation_it != icon_animations_by_key_.end()
                          ? animation_it->second.get()
                          : nullptr);
  }

  // Notifies any ring animation changed callbacks registered for the specified
  // `key` that the associated animation has changed.
  void NotifyRingAnimationChangedForKey(const void* key) {
    auto it = ring_animation_changed_callback_lists_by_key_.find(key);
    if (it == ring_animation_changed_callback_lists_by_key_.end())
      return;
    auto animation_it = ring_animations_by_key_.find(key);
    it->second.Notify(animation_it != ring_animations_by_key_.end()
                          ? animation_it->second.animation.get()
                          : nullptr);
  }

  // Updates animation state for the current `model_` state. If `for_removal` is
  // `true`, the update was triggered by holding space item removal.
  void UpdateAnimations(bool for_removal) {
    // If no `model_` is currently attached, there should be no animations.
    // Animations will be updated if and when a `model_` is attached.
    if (model_ == nullptr) {
      cumulative_progress_ = HoldingSpaceProgress();
      EraseAllAnimations();
      return;
    }

    // Clean up all animations associated with holding space items that are no
    // longer present in the attached `model_`.
    EraseAllAnimationsForKeyIf(base::BindRepeating(
        [](const std::vector<std::unique_ptr<HoldingSpaceItem>>& items,
           const void* controller, const void* key) {
          return key != controller &&
                 !base::Contains(items, key,
                                 &std::unique_ptr<HoldingSpaceItem>::get);
        },
        std::cref(model_->items()), base::Unretained(controller_)));

    HoldingSpaceProgress last_cumulative_progress = cumulative_progress_;
    cumulative_progress_ = HoldingSpaceProgress();

    // Iterate over each holding space item in the attached `model_`.
    for (const auto& item : model_->items()) {
      // If an `item` is not initialized or is not visibly in-progress, it
      // shouldn't contribute to `cumulative_progress_` nor have an animation.
      if (!item->IsInitialized() || item->progress().IsHidden()) {
        EraseAllAnimationsForKey(item.get());
        continue;
      }

      // If the `item` is complete, it should be allowed to continue a pulse
      // animation if one was previously created and started. This would only
      // have happened in response to the `item` transitioning to completion at
      // runtime, as items that are already complete on creation are not
      // animated. Any other type of animation should be cleared. Note that a
      // completed `item` does not contribute to `cumulative_progress_`.
      if (item->progress().IsComplete()) {
        EraseIconAnimationForKey(item.get());
        EraseRingAnimationIfNotOfTypeForKey(
            item.get(), HoldingSpaceProgressRingAnimation::Type::kPulse);
        continue;
      }

      cumulative_progress_ += item->progress();

      // Because the `item` is in-progress, an icon animation should be
      // associated with it (if one does not already exist).
      EnsureIconAnimationForKey(item.get());

      // If the `item` is in an indeterminate state, an indeterminate animation
      // should be associated with it (if one does not already exist).
      if (item->progress().IsIndeterminate()) {
        EnsureRingAnimationOfTypeForKey(
            item.get(),
            HoldingSpaceProgressRingAnimation::Type::kIndeterminate);
        continue;
      }

      // If `item` is not in an indeterminate state, it should not have an
      // associated ring animation.
      EraseRingAnimationForKey(item.get());
    }

    if (cumulative_progress_.IsComplete()) {
      // Because `cumulative_progress_` is complete, the `controller_` should
      // not have an associated icon animation.
      EraseIconAnimationForKey(controller_);

      if (!last_cumulative_progress.IsComplete()) {
        if (for_removal) {
          // If `cumulative_progress_` has just become complete as a result of
          // one or more holding space items being removed, the `controller_`
          // should not have an associated ring animation.
          EraseRingAnimationForKey(controller_);
        } else {
          // If `cumulative_progress_` has just become complete and is *not* due
          // to the removal of one or more holding space items, ensure that a
          // pulse animation is created and started.
          EnsureRingAnimationOfTypeForKey(
              controller_, HoldingSpaceProgressRingAnimation::Type::kPulse);
        }
      } else {
        // If `cumulative_progress_` was already complete, it should be allowed
        // to continue a pulse animation if one was previously created and
        // started. Any other type of ring animation should be cleared.
        EraseRingAnimationIfNotOfTypeForKey(
            controller_, HoldingSpaceProgressRingAnimation::Type::kPulse);
      }
      return;
    }

    // Because `cumulative_progress_` is in-progress, the `controller_` should
    // have an associated icon animation.
    EnsureIconAnimationForKey(controller_);

    // If `cumulative_progress_` is in an indeterminate state, an indeterminate
    // animation should be associated with the `controller_` (if one does not
    // already exist).
    if (cumulative_progress_.IsIndeterminate()) {
      EnsureRingAnimationOfTypeForKey(
          controller_, HoldingSpaceProgressRingAnimation::Type::kIndeterminate);
      return;
    }

    // If `cumulative_progress_` is not in an indeterminate state, the
    // `controller_` should not have an associated ring animation.
    EraseRingAnimationForKey(controller_);
  }

  // Invoked when the specified ring `animation` for the specified `key` has
  // been updated. This is used to clean up finished animations.
  void OnRingAnimationUpdatedForKey(
      const void* key,
      HoldingSpaceProgressRingAnimation* animation) {
    if (animation->IsAnimating())
      return;
    // Once `animation` has finished, it can be removed from the registry. Note
    // that this needs to be posted as it is illegal to delete `animation` from
    // its update callback sequence.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::WeakPtr<ProgressIndicatorAnimationDelegate>&
                   delegate,
               const void* key, HoldingSpaceProgressRingAnimation* animation) {
              if (delegate &&
                  delegate->GetRingAnimationForKey(key) == animation) {
                delegate->EraseRingAnimationForKey(key);
              }
            },
            weak_factory_.GetWeakPtr(), key, animation));
  }

  HoldingSpaceController* const controller_;
  HoldingSpaceModel* model_ = nullptr;

  // The cumulative progress for the attached `model_`, calculated and cached
  // with each call to `UpdateAnimations()`. This is used to determine when
  // cumulative progress changes from an incomplete to a completed state, at
  // which time a pulse animation is created and started.
  HoldingSpaceProgress cumulative_progress_;

  // Mapping of keys to their associated progress icon animations. For
  // cumulative progress, the animation is keyed on a pointer to the holding
  // space `controller_`. For individual item progress, the animation is keyed
  // on a pointer to the holding space item itself.
  std::map<const void*, std::unique_ptr<HoldingSpaceProgressIconAnimation>>
      icon_animations_by_key_;

  // Mapping of keys to their associated icon animation changed callback lists.
  // Whenever an animation for a given key is changed, the callback list for
  // that key will be notified.
  std::map<const void*, ProgressIconAnimationChangedCallbackList>
      icon_animation_changed_callback_lists_by_key_;

  struct SubscribedProgressRingAnimation {
    std::unique_ptr<HoldingSpaceProgressRingAnimation> animation;
    base::CallbackListSubscription subscription;
  };

  // Mapping of keys to their associated progress ring animations. For
  // cumulative progress, the animation is keyed on a pointer to the holding
  // space `controller_`. For individual item progress, the animation is keyed
  // on a pointer to the holding space item itself.
  std::map<const void*, SubscribedProgressRingAnimation>
      ring_animations_by_key_;

  // Mapping of keys to their associated ring animation changed callback lists.
  // Whenever an animation for a given key is changed, the callback list for
  // that key will be notified.
  std::map<const void*, ProgressRingAnimationChangedCallbackList>
      ring_animation_changed_callback_lists_by_key_;

  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      controller_observation_{this};

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};

  base::WeakPtrFactory<ProgressIndicatorAnimationDelegate> weak_factory_{this};
};

// HoldingSpaceAnimationRegistry -----------------------------------------------

HoldingSpaceAnimationRegistry::HoldingSpaceAnimationRegistry() {
  progress_indicator_animation_delegate_ =
      std::make_unique<ProgressIndicatorAnimationDelegate>(
          HoldingSpaceController::Get());

  shell_observation_.Observe(Shell::Get());
}

HoldingSpaceAnimationRegistry::~HoldingSpaceAnimationRegistry() = default;

// static
HoldingSpaceAnimationRegistry* HoldingSpaceAnimationRegistry::GetInstance() {
  auto& instance_owner = GetInstanceOwner();
  if (!instance_owner.get() && Shell::HasInstance())
    instance_owner.reset(new HoldingSpaceAnimationRegistry());
  return instance_owner.get();
}

base::CallbackListSubscription
HoldingSpaceAnimationRegistry::AddProgressIconAnimationChangedCallbackForKey(
    const void* key,
    ProgressIconAnimationChangedCallbackList::CallbackType callback) {
  return progress_indicator_animation_delegate_
      ->AddIconAnimationChangedCallbackForKey(key, std::move(callback));
}

base::CallbackListSubscription
HoldingSpaceAnimationRegistry::AddProgressRingAnimationChangedCallbackForKey(
    const void* key,
    ProgressRingAnimationChangedCallbackList::CallbackType callback) {
  return progress_indicator_animation_delegate_
      ->AddRingAnimationChangedCallbackForKey(key, std::move(callback));
}

HoldingSpaceProgressIconAnimation*
HoldingSpaceAnimationRegistry::GetProgressIconAnimationForKey(const void* key) {
  return progress_indicator_animation_delegate_->GetIconAnimationForKey(key);
}

HoldingSpaceProgressRingAnimation*
HoldingSpaceAnimationRegistry::GetProgressRingAnimationForKey(const void* key) {
  return progress_indicator_animation_delegate_->GetRingAnimationForKey(key);
}

void HoldingSpaceAnimationRegistry::OnShellDestroying() {
  auto& instance_owner = GetInstanceOwner();
  DCHECK_EQ(instance_owner.get(), this);
  instance_owner.reset();  // Deletes `this`.
}

}  // namespace ash
