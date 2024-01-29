// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_DESK_PROFILES_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_DESK_PROFILES_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "base/observer_list.h"

namespace ash {

class ASH_PUBLIC_EXPORT TestDeskProfilesDelegate : public DeskProfilesDelegate {
 public:
  TestDeskProfilesDelegate();
  TestDeskProfilesDelegate(TestDeskProfilesDelegate&) = delete;
  TestDeskProfilesDelegate& operator=(TestDeskProfilesDelegate&) = delete;
  ~TestDeskProfilesDelegate() override;

  // Function to add fake profile. Note: This will invoke
  // Observer::OnProfileUpsert.
  void UpdateTestProfile(LacrosProfileSummary profile);

  // Removes profile by `profile_id`, if profile can't be found, return
  // false. Note: this will invoke Observer::OnProfileRemoved.
  bool RemoveTestProfile(uint64_t profile_id);

  // Set `primary_user_profile_id_` by `profile_id`, if profile can't be found,
  // return false.
  bool SetPrimaryProfileByProfileId(uint64_t profile_id);

  // DeskProfilesDelegate:
  const std::vector<LacrosProfileSummary>& GetProfilesSnapshot() const override;
  const LacrosProfileSummary* GetProfilesSnapshotByProfileId(
      uint64_t profile_id) const override;
  uint64_t GetPrimaryProfileId() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  std::vector<LacrosProfileSummary> profiles_;

  base::ObserverList<Observer> observers_;

  uint64_t primary_user_profile_id_ = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_DESK_PROFILES_DELEGATE_H_
