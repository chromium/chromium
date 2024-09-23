// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NEARBY_SHARE_DELEGATE_H_
#define ASH_PUBLIC_CPP_NEARBY_SHARE_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// This delegate is a singleton used by the
// NearbyShareVisibilityFeaturePodButton in //ash to communicate with the
// NearbySharingService KeyedService in //chrome.
class ASH_PUBLIC_EXPORT NearbyShareDelegate {
 public:
  virtual ~NearbyShareDelegate() = default;

  // Used to determine if NearbyShare has been enabled in the settings app.
  virtual bool IsEnabled() = 0;

  // Used by the pod button to determine whether it should be visible.
  virtual bool IsPodButtonVisible() = 0;

  // Gets the current high visibility state from the NearbySharingService.
  virtual bool IsHighVisibilityOn() = 0;

  // Returns true if EnableHighVisibility() has been called but
  // NearbyShareDelegate has not yet been informed that the request has
  // concluded.
  virtual bool IsEnableHighVisibilityRequestActive() const = 0;

  // If high visibility is on, returns the time when the delegate
  // will turn it off. May return any value if high visibility is off.
  virtual base::TimeTicks HighVisibilityShutoffTime() const = 0;

  // Request high visibility be turned on. If Nearby Share is disabled in prefs,
  // this will instead redirect the user to onboarding.
  virtual void EnableHighVisibility() = 0;

  // Request high visibility be turned off.
  virtual void DisableHighVisibility() = 0;

  // Open the settings page for Nearby Share, Used when the user clicks on the
  // label under the pod button.
  virtual void ShowNearbyShareSettings() const = 0;

  // Returns the icon for Nearby Share. Used by the pod button to
  // display the icon, where `on_icon`=false will return the alternative icon
  // for when the feature is off. Caller should check if the icon is_empty
  // before using.
  virtual const gfx::VectorIcon& GetIcon(bool on_icon) const = 0;

  // Returns the feature name which will be used in feature name placeholder
  // strings, or the empty string if on a non-chrome branded build or the
  // feature flag is disabled.
  virtual std::u16string GetPlaceholderFeatureName() const = 0;

  // Returns the device's current Visibility.
  virtual ::nearby_share::mojom::Visibility GetVisibility() const = 0;

  // Sets the device's Visibility.
  virtual void SetVisibility(::nearby_share::mojom::Visibility visibility) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NEARBY_SHARE_DELEGATE_H_
