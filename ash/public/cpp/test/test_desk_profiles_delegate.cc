// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_desk_profiles_delegate.h"

#include "base/containers/cxx20_erase_vector.h"
#include "base/ranges/algorithm.h"

namespace ash {

TestDeskProfilesDelegate::TestDeskProfilesDelegate() = default;

TestDeskProfilesDelegate::~TestDeskProfilesDelegate() = default;

void TestDeskProfilesDelegate::AddProfile(LacrosProfileSummary profile) {
  if (!GetProfilesSnapshotByProfileId(profile.profile_id)) {
    profiles_.push_back(profile);
  }
}

bool TestDeskProfilesDelegate::RemoveProfilesByProfileId(uint64_t profile_id) {
  return base::EraseIf(profiles_, [&](const auto& profile) {
           return profile.profile_id == profile_id;
         }) != 0;
}

bool TestDeskProfilesDelegate::SetPrimaryProfileByProfileId(
    uint64_t profile_id) {
  if (GetProfilesSnapshotByProfileId(profile_id)) {
    primary_user_profile_id_ = profile_id;
    return true;
  }
  return false;
}

const std::vector<LacrosProfileSummary>&
TestDeskProfilesDelegate::GetProfilesSnapshot() const {
  return profiles_;
}

const LacrosProfileSummary*
TestDeskProfilesDelegate::GetProfilesSnapshotByProfileId(
    uint64_t profile_id) const {
  const auto iter = base::ranges::find(profiles_, profile_id,
                                       &LacrosProfileSummary::profile_id);
  return iter == profiles_.end() ? nullptr : &(*iter);
}

uint64_t TestDeskProfilesDelegate::GetPrimaryProfileId() const {
  return primary_user_profile_id_;
}

void TestDeskProfilesDelegate::AddObserver(Observer* observer) {}

void TestDeskProfilesDelegate::RemoveObserver(Observer* observer) {}

}  // namespace ash
