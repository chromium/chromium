// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NEARBY_SHARE_DELEGATE_H_
#define ASH_PUBLIC_CPP_NEARBY_SHARE_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "base/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

// This delegate is a singleton used by the
// NearbyShareVisibilityFeaturePodButton in //ash to communicate with the
// NearbySharingService KeyedService in //chrome.
class ASH_PUBLIC_EXPORT NearbyShareDelegate {
 public:
  virtual ~NearbyShareDelegate() = default;

  // Used by the pod button to determine whether it should be visible.
  virtual bool IsPodButtonVisible() const = 0;

  // Gets the current high visibility state from the NearbySharingService.
  virtual bool IsHighVisibilityOn() const = 0;

  // If high visibility is on, returns the remaining duration until the delegate
  // will turn it off, or nullopt if high visibility is off.
  virtual base::Optional<base::TimeDelta> RemainingHighVisibilityTime()
      const = 0;

  // Request high visibility be turned on. If Nearby Share is disabled in prefs,
  // this will instead redirect the user to onboarding.
  virtual void EnableHighVisibility() = 0;

  // Request high visibility be turned off.
  virtual void DisableHighVisibility() = 0;

  // Open the settings page for Nearby Share, Used when the user clicks on the
  // label under the pod button.
  virtual void ShowNearbyShareSettings() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NEARBY_SHARE_DELEGATE_H_
