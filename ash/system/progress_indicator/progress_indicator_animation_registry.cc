// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"

namespace ash {
namespace {

// Type aliases ----------------------------------------------------------------

template <typename AnimationType>
using KeyedAnimationWithSubscriptionMap =
    std::map<const void*,
             ProgressIndicatorAnimationRegistry::AnimationWithSubscription<
                 AnimationType>>;

template <typename CallbackListType>
using KeyedAnimationChangedCallbackListMap =
    std::map<const void*, CallbackListType>;

// Helpers ---------------------------------------------------------------------

// Notifies any animation changed callbacks registered for the specified `key`
// that the associated animation has changed.
template <typename AnimationType, typename CallbackListType>
void NotifyAnimationChangedForKey(
    KeyedAnimationWithSubscriptionMap<AnimationType>* animations_by_key,
    KeyedAnimationChangedCallbackListMap<CallbackListType>*
        animation_changed_callback_lists_by_key,
    const void* key) {
  auto callback_lists_it = animation_changed_callback_lists_by_key->find(key);
  if (callback_lists_it == animation_changed_callback_lists_by_key->end())
    return;
  auto animations_it = animations_by_key->find(key);
  callback_lists_it->second.Notify(animations_it != animations_by_key->end()
                                       ? animations_it->second.animation.get()
                                       : nullptr);
}

// Implements:
// * `AddProgressIconAnimationChangedCallbackForKey()`
// * `AddProgressRingAnimationChangedCallbackForKey()`
template <typename CallbackListType>
base::CallbackListSubscription AddAnimationChangedCallbackForKey(
    KeyedAnimationChangedCallbackListMap<CallbackListType>*
        animation_changed_callback_lists_by_key,
    const void* key,
    typename CallbackListType::CallbackType callback) {
  auto it = animation_changed_callback_lists_by_key->find(key);

  // If this is the first time that an animation changed callback is being
  // registered for the specified `key`, set a callback to destroy the created
  // callback list when it becomes empty.
  if (it == animation_changed_callback_lists_by_key->end()) {
    it = animation_changed_callback_lists_by_key
             ->emplace(std::piecewise_construct, std::forward_as_tuple(key),
                       std::forward_as_tuple())
             .first;
    it->second.set_removal_callback(base::BindRepeating(
        [](KeyedAnimationChangedCallbackListMap<CallbackListType>*
               animation_changed_callback_lists_by_key,
           const void* key) {
          auto it = animation_changed_callback_lists_by_key->find(key);
          if (it != animation_changed_callback_lists_by_key->end() &&
              it->second.empty()) {
            animation_changed_callback_lists_by_key->erase(it);
          }
        },
        base::Unretained(animation_changed_callback_lists_by_key),
        base::Unretained(key)));
  }

  return it->second.Add(std::move(callback));
}

// Implements:
// * `GetProgressIconAnimationForKey()`
// * `GetProgressRingAnimationForKey()`
template <typename AnimationType>
AnimationType* GetAnimationForKey(
    KeyedAnimationWithSubscriptionMap<AnimationType>* animations_by_key,
    const void* key) {
  auto it = animations_by_key->find(key);
  return it != animations_by_key->end() ? it->second.animation.get() : nullptr;
}

// Implements:
// * `SetProgressIconAnimationForKey()`
// * `SetProgressRingAnimationForKey()`
template <typename AnimationType, typename CallbackListType>
AnimationType* SetAnimationForKey(
    KeyedAnimationWithSubscriptionMap<AnimationType>* animations_by_key,
    KeyedAnimationChangedCallbackListMap<CallbackListType>*
        animation_changed_callback_lists_by_key,
    const void* key,
    std::unique_ptr<AnimationType> animation) {
  AnimationType* animation_ptr = animation.get();
  if (animation) {
    (*animations_by_key)[key] =
        ProgressIndicatorAnimationRegistry::AnimationWithSubscription<
            AnimationType>{.animation = std::move(animation),
                           .subscription = base::CallbackListSubscription()};
    NotifyAnimationChangedForKey(animations_by_key,
                                 animation_changed_callback_lists_by_key, key);
  } else {
    auto it = animations_by_key->find(key);
    if (it != animations_by_key->end()) {
      animations_by_key->erase(it);
      NotifyAnimationChangedForKey(
          animations_by_key, animation_changed_callback_lists_by_key, key);
    }
  }
  return animation_ptr;
}

}  // namespace

// ProgressIndicatorAnimationRegistry ------------------------------------------

ProgressIndicatorAnimationRegistry::ProgressIndicatorAnimationRegistry() =
    default;

ProgressIndicatorAnimationRegistry::~ProgressIndicatorAnimationRegistry() =
    default;

base::CallbackListSubscription ProgressIndicatorAnimationRegistry::
    AddProgressIconAnimationChangedCallbackForKey(
        const void* key,
        ProgressIconAnimationChangedCallbackList::CallbackType callback) {
  return AddAnimationChangedCallbackForKey(
      &icon_animation_changed_callback_lists_by_key_, key, std::move(callback));
}

base::CallbackListSubscription ProgressIndicatorAnimationRegistry::
    AddProgressRingAnimationChangedCallbackForKey(
        const void* key,
        ProgressRingAnimationChangedCallbackList::CallbackType callback) {
  return AddAnimationChangedCallbackForKey(
      &ring_animation_changed_callback_lists_by_key_, key, std::move(callback));
}

HoldingSpaceProgressIconAnimation*
ProgressIndicatorAnimationRegistry::GetProgressIconAnimationForKey(
    const void* key) {
  return GetAnimationForKey(&icon_animations_by_key_, key);
}

HoldingSpaceProgressRingAnimation*
ProgressIndicatorAnimationRegistry::GetProgressRingAnimationForKey(
    const void* key) {
  return GetAnimationForKey(&ring_animations_by_key_, key);
}

HoldingSpaceProgressIconAnimation*
ProgressIndicatorAnimationRegistry::SetProgressIconAnimationForKey(
    const void* key,
    std::unique_ptr<HoldingSpaceProgressIconAnimation> animation) {
  return SetAnimationForKey(&icon_animations_by_key_,
                            &icon_animation_changed_callback_lists_by_key_, key,
                            std::move(animation));
}

HoldingSpaceProgressRingAnimation*
ProgressIndicatorAnimationRegistry::SetProgressRingAnimationForKey(
    const void* key,
    std::unique_ptr<HoldingSpaceProgressRingAnimation> animation) {
  return SetAnimationForKey(&ring_animations_by_key_,
                            &ring_animation_changed_callback_lists_by_key_, key,
                            std::move(animation));
}

}  // namespace ash
