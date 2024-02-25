// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/fake_nearby_share_profile_info_provider.h"

FakeNearbyShareProfileInfoProvider::FakeNearbyShareProfileInfoProvider() =
    default;

FakeNearbyShareProfileInfoProvider::~FakeNearbyShareProfileInfoProvider() =
    default;

std::optional<std::u16string> FakeNearbyShareProfileInfoProvider::GetGivenName()
    const {
  return given_name_;
}

std::optional<std::string>
FakeNearbyShareProfileInfoProvider::GetProfileUserName() const {
  return profile_user_name_;
}
