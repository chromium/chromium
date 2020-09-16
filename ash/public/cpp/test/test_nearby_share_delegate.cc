// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_nearby_share_delegate.h"

namespace ash {

TestNearbyShareDelegate::TestNearbyShareDelegate() = default;

TestNearbyShareDelegate::~TestNearbyShareDelegate() = default;

bool TestNearbyShareDelegate::IsPodButtonVisible() const {
  return false;
}

bool TestNearbyShareDelegate::IsHighVisibilityOn() const {
  return false;
}

base::Optional<base::TimeDelta>
TestNearbyShareDelegate::RemainingHighVisibilityTime() const {
  return base::nullopt;
}

void TestNearbyShareDelegate::EnableHighVisibility() {}

void TestNearbyShareDelegate::DisableHighVisibility() {}

void TestNearbyShareDelegate::ShowNearbyShareSettings() const {}

}  // namespace ash
