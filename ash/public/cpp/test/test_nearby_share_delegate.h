// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_NEARBY_SHARE_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_NEARBY_SHARE_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/nearby_share_delegate.h"

namespace ash {

// A NearbyShareDelegate that does nothing. Used by TestShellDelegate.
class ASH_PUBLIC_EXPORT TestNearbyShareDelegate : public NearbyShareDelegate {
 public:
  TestNearbyShareDelegate();
  ~TestNearbyShareDelegate() override;

  TestNearbyShareDelegate(TestNearbyShareDelegate&) = delete;
  TestNearbyShareDelegate& operator=(TestNearbyShareDelegate&) = delete;

  // NearbyShareDelegate
  bool IsPodButtonVisible() override;
  bool IsHighVisibilityOn() override;
  base::Optional<base::TimeDelta> RemainingHighVisibilityTime() override;
  void EnableHighVisibility() override;
  void DisableHighVisibility() override;
  void ShowNearbyShareSettings() const override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_NEARBY_SHARE_DELEGATE_H_
