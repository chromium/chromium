// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_animation_registry.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_item_updated_fields.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/shell.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"

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
  ProgressIndicatorAnimationDelegate(
      ProgressIndicatorAnimationRegistry* registry,
      HoldingSpaceController* controller)
      : registry_(registry), controller_(controller) {
    controller_observation_.Observe(controller_.get());
    if (controller_->model())
      OnHoldingSpaceModelAttached(controller_->model());
  }

  ProgressIndicatorAnimationDelegate(
      const ProgressIndicatorAnimationDelegate&) = delete;
  ProgressIndicatorAnimationDelegate& operator=(
      const ProgressIndicatorAnimationDelegate&) = delete;
  ~ProgressIndicatorAnimationDelegate() override = default;

 private:
  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override {
    model_ = model;
    model_observation_.Observe(model_.get());
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
    const bool removed_in_progress_item =
        base::ranges::any_of(items, [](const HoldingSpaceItem* item) {
          return item->IsInitialized() && !item->progress().IsComplete();
        });
    if (removed_in_progress_item)
      UpdateAnimations(/*for_removal=*/true);
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    UpdateAnimations(/*for_removal=*/false);
  }

  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override {
    // The `item` update can be safely ignored if progress has not been updated.
    if (!updated_fields.previous_progress) {
      return;
    }

    // If `item` has just progressed to completion, ensure that a pulse
    // animation is created and started.
    if (item->progress().IsComplete()) {
      EnsureRingAnimationOfTypeForKey(AsAnimationKey(item),
                                      ProgressRingAnimation::Type::kPulse);
    }

    UpdateAnimations(/*for_removal=*/false);
  }

  // Erases the ring animation for the specified `key` if it is not of the
  // desired `type`, notifying any animation changed callbacks.
  void EraseRingAnimationIfNotOfTypeForKey(AnimationKey key,
                                           ProgressRingAnimation::Type type) {
    auto* ring_animation = registry_->GetProgressRingAnimationForKey(key);
    if (ring_animation && ring_animation->type() != type)
      registry_->SetProgressRingAnimationForKey(key, nullptr);
  }

  // Ensures that the icon animation for the specified `key` exists. If
  // necessary, a new animation is created and started, notifying any animation
  // changed callbacks.
  void EnsureIconAnimationForKey(AnimationKey key) {
    if (registry_->GetProgressIconAnimationForKey(key))
      return;

    auto* animation = registry_->SetProgressIconAnimationForKey(
        key, ProgressIconAnimation::Create());

    // Only `Start()` the `animation` if it is associated with the holding space
    // `controller_`. In all other cases, the `animation` is associated with a
    // holding space item and will be started after the associated holding space
    // tray item preview has had the opportunity to animate in.
    if (key == AsAnimationKey(controller_)) {
      animation->Start();
    }
  }

  // Ensures that the ring animation for the specified `key` is of the desired
  // `type`. If necessary, a new animation is created and started, notifying any
  // animation changed callbacks.
  void EnsureRingAnimationOfTypeForKey(AnimationKey key,
                                       ProgressRingAnimation::Type type) {
    auto* ring_animation = registry_->GetProgressRingAnimationForKey(key);
    if (ring_animation && ring_animation->type() == type)
      return;

    auto animation = ProgressRingAnimation::CreateOfType(type);
    animation->AddUnsafeAnimationUpdatedCallback(base::BindRepeating(
        &ProgressIndicatorAnimationDelegate::OnRingAnimationUpdatedForKey,
        base::Unretained(this), key, animation.get()));

    registry_->SetProgressRingAnimationForKey(key, std::move(animation))
        ->Start();
  }

  // Updates animation state for the current `model_` state. If `for_removal` is
  // `true`, the update was triggered by holding space item removal.
  void UpdateAnimations(bool for_removal) {
    // If no `model_` is currently attached, there should be no animations.
    // Animations will be updated if and when a `model_` is attached.
    if (model_ == nullptr) {
      cumulative_progress_ = HoldingSpaceProgress();
      registry_->EraseAllAnimations();
      return;
    }

    const auto controller_key = AsAnimationKey(controller_);

    // Clean up all animations associated with holding space items that are no
    // longer present in the attached `model_`.
    registry_->EraseAllAnimationsForKeyIf([&](AnimationKey key) {
      return key != controller_key &&
             !base::Contains(model_->items(), key,
                             [](const std::unique_ptr<HoldingSpaceItem>& item) {
                               return AsAnimationKey(item.get());
                             });
    });

    HoldingSpaceProgress last_cumulative_progress = cumulative_progress_;
    cumulative_progress_ = HoldingSpaceProgress();

    // Iterate over each holding space item in the attached `model_`.
    for (const auto& item : model_->items()) {
      const auto item_key = AsAnimationKey(item.get());

      // If an `item` is not initialized or is not visibly in-progress, it
      // shouldn't contribute to `cumulative_progress_` nor have an animation.
      if (!item->IsInitialized() || item->progress().IsHidden()) {
        registry_->EraseAllAnimationsForKey(item_key);
        continue;
      }

      // If the `item` is complete, it should be allowed to continue a pulse
      // animation if one was previously created and started. This would only
      // have happened in response to the `item` transitioning to completion at
      // runtime, as items that are already complete on creation are not
      // animated. Any other type of animation should be cleared. Note that a
      // completed `item` does not contribute to `cumulative_progress_`.
      if (item->progress().IsComplete()) {
        registry_->SetProgressIconAnimationForKey(item_key, nullptr);
        EraseRingAnimationIfNotOfTypeForKey(
            item_key, ProgressRingAnimation::Type::kPulse);
        continue;
      }

      cumulative_progress_ += item->progress();

      // Because the `item` is in-progress, an icon animation should be
      // associated with it (if one does not already exist).
      EnsureIconAnimationForKey(item_key);

      // If the `item` is in an indeterminate state, an indeterminate animation
      // should be associated with it (if one does not already exist).
      if (item->progress().IsIndeterminate()) {
        EnsureRingAnimationOfTypeForKey(
            item_key, ProgressRingAnimation::Type::kIndeterminate);
        continue;
      }

      // If `item` is not in an indeterminate state, it should not have an
      // associated ring animation.
      registry_->SetProgressRingAnimationForKey(item_key, nullptr);
    }

    if (cumulative_progress_.IsComplete()) {
      // Because `cumulative_progress_` is complete, the `controller_` should
      // not have an associated icon animation.
      registry_->SetProgressIconAnimationForKey(controller_key, nullptr);

      if (!last_cumulative_progress.IsComplete()) {
        if (for_removal) {
          // If `cumulative_progress_` has just become complete as a result of
          // one or more holding space items being removed, the `controller_`
          // should not have an associated ring animation.
          registry_->SetProgressRingAnimationForKey(controller_key, nullptr);
        } else {
          // If `cumulative_progress_` has just become complete and is *not* due
          // to the removal of one or more holding space items, ensure that a
          // pulse animation is created and started.
          EnsureRingAnimationOfTypeForKey(controller_key,
                                          ProgressRingAnimation::Type::kPulse);
        }
      } else {
        // If `cumulative_progress_` was already complete, it should be allowed
        // to continue a pulse animation if one was previously created and
        // started. Any other type of ring animation should be cleared.
        EraseRingAnimationIfNotOfTypeForKey(
            controller_key, ProgressRingAnimation::Type::kPulse);
      }
      return;
    }

    // Because `cumulative_progress_` is in-progress, the `controller_` should
    // have an associated icon animation.
    EnsureIconAnimationForKey(controller_key);

    // If `cumulative_progress_` is in an indeterminate state, an indeterminate
    // animation should be associated with the `controller_` (if one does not
    // already exist).
    if (cumulative_progress_.IsIndeterminate()) {
      EnsureRingAnimationOfTypeForKey(
          controller_key, ProgressRingAnimation::Type::kIndeterminate);
      return;
    }

    // If `cumulative_progress_` is not in an indeterminate state, the
    // `controller_` should not have an associated ring animation.
    registry_->SetProgressRingAnimationForKey(controller_key, nullptr);
  }

  // Invoked when the specified ring `animation` for the specified `key` has
  // been updated. This is used to clean up finished animations.
  void OnRingAnimationUpdatedForKey(AnimationKey key,
                                    ProgressRingAnimation* animation) {
    if (animation->IsAnimating())
      return;
    // Once `animation` has finished, it can be removed from the registry. Note
    // that this needs to be posted as it is illegal to delete `animation` from
    // its update callback sequence.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::WeakPtr<ProgressIndicatorAnimationDelegate>& self,
               AnimationKey key,
               MayBeDangling<ProgressRingAnimation> animation) {
              if (!self) {
                return;
              }
              auto* registry = self->registry_.get();
              if (registry->GetProgressRingAnimationForKey(key) == animation) {
                registry->SetProgressRingAnimationForKey(key, nullptr);
              }
            },
            weak_factory_.GetWeakPtr(), key, base::UnsafeDangling(animation)));
  }

  const raw_ptr<ProgressIndicatorAnimationRegistry, LeakedDanglingUntriaged>
      registry_;
  const raw_ptr<HoldingSpaceController, LeakedDanglingUntriaged> controller_;
  raw_ptr<HoldingSpaceModel, LeakedDanglingUntriaged> model_ = nullptr;

  // The cumulative progress for the attached `model_`, calculated and cached
  // with each call to `UpdateAnimations()`. This is used to determine when
  // cumulative progress changes from an incomplete to a completed state, at
  // which time a pulse animation is created and started.
  HoldingSpaceProgress cumulative_progress_;

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
          this, HoldingSpaceController::Get());

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

void HoldingSpaceAnimationRegistry::OnShellDestroying() {
  auto& instance_owner = GetInstanceOwner();
  DCHECK_EQ(instance_owner.get(), this);
  instance_owner.reset();  // Deletes `this`.
}

}  // namespace ash
