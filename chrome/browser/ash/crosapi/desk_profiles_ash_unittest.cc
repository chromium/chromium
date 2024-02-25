// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_profiles_ash.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace crosapi {

using testing::ElementsAre;

MATCHER_P2(EqualsProfile, profile_id, name, "") {
  return profile_id == arg.profile_id && name == arg.name;
}

class DeskProfileObserver : public ash::DeskProfilesDelegate::Observer {
 public:
  void OnProfileUpsert(const ash::LacrosProfileSummary& summary) override {
    updates_.push_back(summary);
  }

  void OnProfileRemoved(uint64_t profile_id) override {
    removes_.push_back(profile_id);
  }

  std::vector<ash::LacrosProfileSummary> ConsumeUpdates() {
    return std::exchange(updates_, {});
  }

  std::vector<uint64_t> ConsumeRemoves() { return std::exchange(removes_, {}); }

 private:
  // Tracks received updates. Cleared by `ConsumeUpdates`.
  std::vector<ash::LacrosProfileSummary> updates_;

  // Tracks received removals. Cleared by `ConsumeRemoves`.
  std::vector<uint64_t> removes_;
};

class DeskProfilesAshTest : public testing::Test {
 public:
  ash::DeskProfilesDelegate& delegate() { return desk_profiles_ash_; }

  void SendProfileUpdate(
      const std::vector<std::pair<uint64_t, std::string>>& profiles) {
    // Convert to mojom format.
    std::vector<mojom::LacrosProfileSummaryPtr> mojom_profiles;
    for (const auto& [profile_id, name] : profiles) {
      auto summary = mojom::LacrosProfileSummary::New();
      summary->profile_id = profile_id;
      summary->name = name;

      mojom_profiles.push_back(std::move(summary));
    }

    desk_profiles_observer().OnProfileUpsert(std::move(mojom_profiles));
  }

  void SendProfileRemoved(uint64_t profile_id) {
    desk_profiles_observer().OnProfileRemoved(profile_id);
  }

 private:
  mojom::DeskProfileObserver& desk_profiles_observer() {
    return desk_profiles_ash_;
  }

  DeskProfilesAsh desk_profiles_ash_;
};

TEST_F(DeskProfilesAshTest, GetSnapshot) {
  // Initial empty state.
  EXPECT_THAT(delegate().GetProfilesSnapshot(), ElementsAre());

  // Snapshot order should match update order.
  SendProfileUpdate({{124u, "profile 1"}, {102u, "profile 2"}});
  EXPECT_THAT(delegate().GetProfilesSnapshot(),
              ElementsAre(EqualsProfile(124u, u"profile 1"),
                          EqualsProfile(102u, u"profile 2")));

  // Updated profile remains in place, new profile goes at the end.
  SendProfileUpdate({{124u, "profile 1 renamed"}, {100u, "profile 3"}});
  EXPECT_THAT(delegate().GetProfilesSnapshot(),
              ElementsAre(EqualsProfile(124u, u"profile 1 renamed"),
                          EqualsProfile(102u, u"profile 2"),
                          EqualsProfile(100u, u"profile 3")));

  // Removing a profile does not change the order among other profiles.
  SendProfileRemoved(102);
  EXPECT_THAT(delegate().GetProfilesSnapshot(),
              ElementsAre(EqualsProfile(124u, u"profile 1 renamed"),
                          EqualsProfile(100u, u"profile 3")));
}

TEST_F(DeskProfilesAshTest, ObserverTest) {
  DeskProfileObserver observer;
  delegate().AddObserver(&observer);

  SendProfileUpdate({{106u, "profile 2"}, {104u, "profile 3"}});
  EXPECT_THAT(observer.ConsumeUpdates(),
              ElementsAre(EqualsProfile(106u, u"profile 2"),
                          EqualsProfile(104u, u"profile 3")));
  EXPECT_THAT(observer.ConsumeRemoves(), ElementsAre());

  SendProfileUpdate({{106u, "profile 2 renamed"}});
  EXPECT_THAT(observer.ConsumeUpdates(),
              ElementsAre(EqualsProfile(106u, u"profile 2 renamed")));

  // Removal of unknown profile is not propagated to observers.
  SendProfileRemoved(102u);
  EXPECT_THAT(observer.ConsumeUpdates(), ElementsAre());
  EXPECT_THAT(observer.ConsumeRemoves(), ElementsAre());

  SendProfileRemoved(106u);
  EXPECT_THAT(observer.ConsumeUpdates(), ElementsAre());
  EXPECT_THAT(observer.ConsumeRemoves(), ElementsAre(106u));
}

}  // namespace crosapi
