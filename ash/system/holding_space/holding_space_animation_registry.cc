// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
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

// HoldingSpaceAnimationRegistry::ProgressRingAnimationDelegate ----------------

// The delegate of `HoldingSpaceAnimationRegistry` responsible for creating and
// curating progress ring animations based on holding space model state.
class HoldingSpaceAnimationRegistry::ProgressRingAnimationDelegate
    : public HoldingSpaceControllerObserver,
      public HoldingSpaceModelObserver {
 public:
  explicit ProgressRingAnimationDelegate(HoldingSpaceController* controller)
      : controller_(controller) {
    controller_observation_.Observe(controller_);
    if (controller_->model())
      OnHoldingSpaceModelAttached(controller_->model());
  }

  ProgressRingAnimationDelegate(const ProgressRingAnimationDelegate&) = delete;
  ProgressRingAnimationDelegate& operator=(
      const ProgressRingAnimationDelegate&) = delete;
  ~ProgressRingAnimationDelegate() override = default;

  // Adds the specified `callback` to be notified of changes to the animation
  // associated with the specified `key`. The `callback` will continue to
  // receive events so long as both `this` and the returned subscription exist.
  ProgressRingAnimationChangedCallbackList::Subscription
  AddAnimationChangedCallbackForKey(
      const void* key,
      ProgressRingAnimationChangedCallbackList::CallbackType callback) {
    auto it = animation_changed_callback_lists_by_key_.find(key);

    // If this is the first time that an animation changed callback is being
    // registered for the specified `key`, set a callback to destroy the
    // created callback list when it becomes empty.
    if (it == animation_changed_callback_lists_by_key_.end()) {
      it = animation_changed_callback_lists_by_key_
               .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                        std::forward_as_tuple())
               .first;
      it->second.set_removal_callback(base::BindRepeating(
          &ProgressRingAnimationDelegate::
              EraseAnimationChangedCallbackListForKeyIfEmpty,
          base::Unretained(this), base::Unretained(key)));
    }

    return it->second.Add(std::move(callback));
  }

  // Returns the registered animation for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  HoldingSpaceProgressRingAnimation* GetAnimationForKey(const void* key) {
    auto it = animations_by_key_.find(key);
    return it != animations_by_key_.end() ? it->second.animation.get()
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
      EnsureAnimationOfTypeForKey(
          item, HoldingSpaceProgressRingAnimation::Type::kPulse);
    }

    UpdateAnimations(/*for_removal=*/false);
  }

  // Erases all animations, notifying any animation changed callbacks.
  void EraseAllAnimations() {
    while (!animations_by_key_.empty()) {
      auto it = animations_by_key_.begin();
      const void* key = it->first;
      animations_by_key_.erase(it);
      NotifyAnimationChangedForKey(key);
    }
  }

  // Erases the animation for the specified `key`, notifying any animation
  // changed callbacks.
  void EraseAnimationForKey(const void* key) {
    auto it = animations_by_key_.find(key);
    if (it == animations_by_key_.end())
      return;
    animations_by_key_.erase(it);
    NotifyAnimationChangedForKey(key);
  }

  // Erases any animation for which `predicate` returns `true`, notifying any
  // animation changed callbacks.
  void EraseAnimationIf(
      base::RepeatingCallback<bool(const void* key)> predicate) {
    std::set<const void*> keys_to_erase;
    for (const auto& animation_by_key : animations_by_key_) {
      const void* key = animation_by_key.first;
      if (predicate.Run(key))
        keys_to_erase.insert(key);
    }
    for (const void* key : keys_to_erase)
      EraseAnimationForKey(key);
  }

  // Erases the animation for the specified `key` if it is not of the desired
  // `type`, notifying any animation changed callbacks.
  void EraseAnimationIfNotOfTypeForKey(
      const void* key,
      HoldingSpaceProgressRingAnimation::Type type) {
    auto it = animations_by_key_.find(key);
    if (it != animations_by_key_.end() && it->second.animation->type() != type)
      EraseAnimationForKey(key);
  }

  // Erases the animation callback list for the specified `key` if it is empty.
  void EraseAnimationChangedCallbackListForKeyIfEmpty(const void* key) {
    auto it = animation_changed_callback_lists_by_key_.find(key);
    if (it == animation_changed_callback_lists_by_key_.end())
      return;
    if (it->second.empty())
      animation_changed_callback_lists_by_key_.erase(it);
  }

  // Ensures that the animation for the specified `key` is of the desired
  // `type`. If necessary, a new animation is created and started, notifying any
  // animation changed callbacks.
  void EnsureAnimationOfTypeForKey(
      const void* key,
      HoldingSpaceProgressRingAnimation::Type type) {
    auto it = animations_by_key_.find(key);
    if (it != animations_by_key_.end() && it->second.animation->type() == type)
      return;

    auto animation = HoldingSpaceProgressRingAnimation::CreateOfType(type);

    auto subscription =
        animation->AddAnimationUpdatedCallback(base::BindRepeating(
            &ProgressRingAnimationDelegate::OnAnimationUpdatedForKey,
            base::Unretained(this), key, animation.get()));

    auto subscribed_animation = SubscribedProgressRingAnimation{
        .animation = std::move(animation),
        .subscription = std::move(subscription)};

    animations_by_key_.emplace(key, std::move(subscribed_animation))
        .first->second.animation->Start();

    NotifyAnimationChangedForKey(key);
  }

  // Notifies any animation changed callbacks registered for the specified `key`
  // that the associated animation has changed.
  void NotifyAnimationChangedForKey(const void* key) {
    auto it = animation_changed_callback_lists_by_key_.find(key);
    if (it == animation_changed_callback_lists_by_key_.end())
      return;
    auto animation_it = animations_by_key_.find(key);
    it->second.Notify(animation_it != animations_by_key_.end()
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

    // Clean up any animations associated with holding space items that are no
    // longer present in the attached `model_`.
    EraseAnimationIf(base::BindRepeating(
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
        EraseAnimationForKey(item.get());
        continue;
      }

      // If the `item` is complete, it should be allowed to continue a pulse
      // animation if one was previously created and started. This would only
      // have happened in response to the `item` transitioning to completion at
      // runtime, as items that are already complete on creation are not
      // animated. Any other type of animation should be cleared. Note that a
      // completed `item` does not contribute to `cumulative_progress_`.
      if (item->progress().IsComplete()) {
        EraseAnimationIfNotOfTypeForKey(
            item.get(), HoldingSpaceProgressRingAnimation::Type::kPulse);
        continue;
      }

      cumulative_progress_ += item->progress();

      // If the `item` is in an indeterminate state, an indeterminate animation
      // should be associated with it (if one does not already exist).
      if (item->progress().IsIndeterminate()) {
        EnsureAnimationOfTypeForKey(
            item.get(),
            HoldingSpaceProgressRingAnimation::Type::kIndeterminate);
        continue;
      }

      // If `item` is not in an indeterminate state, it should not have an
      // associated animation.
      EraseAnimationForKey(item.get());
    }

    if (cumulative_progress_.IsComplete()) {
      if (!last_cumulative_progress.IsComplete()) {
        if (for_removal) {
          // If `cumulative_progress_` has just become complete as a result of
          // one or more holding space items being removed, the `controller_`
          // should not have an associated animation.
          EraseAnimationForKey(controller_);
        } else {
          // If `cumulative_progress_` has just become complete and is *not* due
          // to the removal of one or more holding space items, ensure that a
          // pulse animation is created and started.
          EnsureAnimationOfTypeForKey(
              controller_, HoldingSpaceProgressRingAnimation::Type::kPulse);
        }
      } else {
        // If `cumulative_progress_` was already complete, it should be allowed
        // to continue a pulse animation if one was previously created and
        // started. Any other type of animation should be cleared.
        EraseAnimationIfNotOfTypeForKey(
            controller_, HoldingSpaceProgressRingAnimation::Type::kPulse);
      }
      return;
    }

    // If `cumulative_progress_` is in an indeterminate state, an indeterminate
    // animation should be associated with the `controller_` (if one does not
    // already exist).
    if (cumulative_progress_.IsIndeterminate()) {
      EnsureAnimationOfTypeForKey(
          controller_, HoldingSpaceProgressRingAnimation::Type::kIndeterminate);
      return;
    }

    // If `cumulative_progress_` is not in an indeterminate state, the
    // `controller_` should not have an associated animation.
    EraseAnimationForKey(controller_);
  }

  // Invoked when the specified `animation` for the specified `key` has been
  // updated. This is used to clean up finished animations.
  void OnAnimationUpdatedForKey(const void* key,
                                HoldingSpaceProgressRingAnimation* animation) {
    if (animation->IsAnimating())
      return;
    // Once `animation` has finished, it can be removed from the registry. Note
    // that this needs to be posted as it is illegal to delete `animation` from
    // its update callback sequence.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::WeakPtr<ProgressRingAnimationDelegate>& delegate,
               const void* key, HoldingSpaceProgressRingAnimation* animation) {
              if (delegate && delegate->GetAnimationForKey(key) == animation)
                delegate->EraseAnimationForKey(key);
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

  struct SubscribedProgressRingAnimation {
    std::unique_ptr<HoldingSpaceProgressRingAnimation> animation;
    base::RepeatingClosureList::Subscription subscription;
  };

  // Mapping of keys to their associated progress ring animations. For
  // cumulative progress, the animation is keyed on a pointer to the holding
  // space `controller_`. For individual item progress, the animation is keyed
  // on a pointer to the holding space item itself.
  std::map<const void*, SubscribedProgressRingAnimation> animations_by_key_;

  // Mapping of keys to their associated animation changed callback lists.
  // Whenever an animation for a given key is changed, the callback list for
  // that key will be notified.
  std::map<const void*, ProgressRingAnimationChangedCallbackList>
      animation_changed_callback_lists_by_key_;

  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      controller_observation_{this};

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};

  base::WeakPtrFactory<ProgressRingAnimationDelegate> weak_factory_{this};
};

// HoldingSpaceAnimationRegistry -----------------------------------------------

HoldingSpaceAnimationRegistry::HoldingSpaceAnimationRegistry() {
  progress_ring_animation_delegate_ =
      std::make_unique<ProgressRingAnimationDelegate>(
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

HoldingSpaceAnimationRegistry::ProgressRingAnimationChangedCallbackList::
    Subscription
    HoldingSpaceAnimationRegistry::
        AddProgressRingAnimationChangedCallbackForKey(
            const void* key,
            ProgressRingAnimationChangedCallbackList::CallbackType callback) {
  return progress_ring_animation_delegate_->AddAnimationChangedCallbackForKey(
      key, std::move(callback));
}

HoldingSpaceProgressRingAnimation*
HoldingSpaceAnimationRegistry::GetProgressRingAnimationForKey(const void* key) {
  return progress_ring_animation_delegate_->GetAnimationForKey(key);
}

void HoldingSpaceAnimationRegistry::OnShellDestroying() {
  auto& instance_owner = GetInstanceOwner();
  DCHECK_EQ(instance_owner.get(), this);
  instance_owner.reset();  // Deletes `this`.
}

}  // namespace ash
