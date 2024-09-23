// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_desk_profiles_delegate.h"

#include <vector>

#include "base/ranges/algorithm.h"

namespace ash {

TestDeskProfilesDelegate::TestDeskProfilesDelegate() = default;

TestDeskProfilesDelegate::~TestDeskProfilesDelegate() = default;

void TestDeskProfilesDelegate::UpdateTestProfile(LacrosProfileSummary profile) {
  auto* summary = const_cast<LacrosProfileSummary*>(
      GetProfilesSnapshotByProfileId(profile.profile_id));
  if (!summary) {
    summary = &profiles_.emplace_back(std::move(profile));
  }

  for (auto& observer : observers_) {
    observer.OnProfileUpsert(*summary);
  }
}

bool TestDeskProfilesDelegate::RemoveTestProfile(uint64_t profile_id) {
  CHECK(profile_id != primary_user_profile_id_);

  if (std::erase_if(profiles_, [&](const auto& profile) {
        return profile.profile_id == profile_id;
      })) {
    for (auto& observer : observers_) {
      observer.OnProfileRemoved(profile_id);
    }
    return true;
  }

  return false;
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
  if (profile_id == 0) {
    profile_id = primary_user_profile_id_;
  }

  const auto iter = base::ranges::find(profiles_, profile_id,
                                       &LacrosProfileSummary::profile_id);
  return iter == profiles_.end() ? nullptr : &(*iter);
}

uint64_t TestDeskProfilesDelegate::GetPrimaryProfileId() const {
  return primary_user_profile_id_;
}

void TestDeskProfilesDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TestDeskProfilesDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
