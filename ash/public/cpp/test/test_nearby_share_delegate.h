// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_NEARBY_SHARE_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_NEARBY_SHARE_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/nearby_share_delegate.h"
#include "base/time/time.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"

namespace ash {

// A NearbyShareDelegate that does nothing. Used by TestShellDelegate.
class ASH_PUBLIC_EXPORT TestNearbyShareDelegate : public NearbyShareDelegate {
 public:
  enum Method { kEnableHighVisibility, kDisableHighVisibility };

  TestNearbyShareDelegate();
  ~TestNearbyShareDelegate() override;

  TestNearbyShareDelegate(TestNearbyShareDelegate&) = delete;
  TestNearbyShareDelegate& operator=(TestNearbyShareDelegate&) = delete;

  // NearbyShareDelegate
  bool IsEnabled() override;
  bool IsPodButtonVisible() override;
  bool IsHighVisibilityOn() override;
  bool IsEnableHighVisibilityRequestActive() const override;
  base::TimeTicks HighVisibilityShutoffTime() const override;
  void EnableHighVisibility() override;
  void DisableHighVisibility() override;
  void ShowNearbyShareSettings() const override;
  const gfx::VectorIcon& GetIcon(bool on_icon) const override;
  std::u16string GetPlaceholderFeatureName() const override;
  ::nearby_share::mojom::Visibility GetVisibility() const override;
  void SetVisibility(::nearby_share::mojom::Visibility visibility) override;

  void set_is_enabled(bool enabled) { is_enabled_ = enabled; }

  void set_is_pod_button_visible(bool visible) {
    is_pod_button_visible_ = visible;
  }

  void set_is_enable_high_visibility_request_active(
      bool is_enable_high_visibility_request_active) {
    is_enable_high_visibility_request_active_ =
        is_enable_high_visibility_request_active;
  }

  void set_is_high_visibility_on(bool on) { is_high_visibility_on_ = on; }

  void set_high_visibility_shutoff_time(base::TimeTicks time) {
    high_visibility_shutoff_time_ = time;
  }

  void set_visibility(::nearby_share::mojom::Visibility visibility) {
    visibility_ = visibility;
  }

  std::vector<Method>& method_calls() { return method_calls_; }

 private:
  bool is_enabled_ = true;
  bool is_pod_button_visible_ = false;
  bool is_enable_high_visibility_request_active_ = false;
  bool is_high_visibility_on_ = false;
  base::TimeTicks high_visibility_shutoff_time_;
  std::vector<Method> method_calls_;
  ::nearby_share::mojom::Visibility visibility_ =
      ::nearby_share::mojom::Visibility::kYourDevices;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_NEARBY_SHARE_DELEGATE_H_
