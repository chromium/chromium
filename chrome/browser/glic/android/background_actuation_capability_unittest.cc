// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/background_actuation_capability.h"

#include <memory>
#include <optional>

#include "base/android/application_status_listener.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class BackgroundActuationCapabilityTest : public testing::Test {
 public:
  BackgroundActuationCapabilityTest() {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kGlicLastCanActuateInBackground, false);
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    SetCanActuateInBackground(true);
  }

  ~BackgroundActuationCapabilityTest() override {
    BackgroundActuationCapability::SetCanActuateInBackgroundForTesting(
        std::nullopt);
  }

 protected:
  std::unique_ptr<BackgroundActuationCapability> Create() {
    return std::make_unique<BackgroundActuationCapability>(
        &prefs_, identity_test_env_.identity_manager(),
        base::BindLambdaForTesting([this]() { ++changed_count_; }));
  }

  void SetCanActuateInBackground(bool can) {
    BackgroundActuationCapability::SetCanActuateInBackgroundForTesting(can);
  }

  bool GetPersistedValue() {
    return prefs_.GetBoolean(prefs::kGlicLastCanActuateInBackground);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  int changed_count_ = 0;
};

TEST_F(BackgroundActuationCapabilityTest, ReadIsDeferredAtConstruction) {
  auto capability = Create();
  // The constructor doesn't read the value; it starts from the persisted one.
  EXPECT_FALSE(capability->can_actuate_in_background());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return capability->can_actuate_in_background(); }));
  EXPECT_EQ(changed_count_, 1);
  EXPECT_TRUE(GetPersistedValue());
}

TEST_F(BackgroundActuationCapabilityTest, SeedsFromPersistedValue) {
  prefs_.SetBoolean(prefs::kGlicLastCanActuateInBackground, true);
  auto capability = Create();
  EXPECT_TRUE(capability->can_actuate_in_background());
  EXPECT_EQ(changed_count_, 0);
}

TEST_F(BackgroundActuationCapabilityTest, StalePersistedValueIsCorrected) {
  // The device could actuate in the background last time, but can't anymore
  // (e.g. notifications were turned off while Chrome wasn't running).
  prefs_.SetBoolean(prefs::kGlicLastCanActuateInBackground, true);
  SetCanActuateInBackground(false);
  auto capability = Create();
  ASSERT_TRUE(capability->can_actuate_in_background());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !capability->can_actuate_in_background(); }));
  EXPECT_EQ(changed_count_, 1);
  EXPECT_FALSE(GetPersistedValue());
}

TEST_F(BackgroundActuationCapabilityTest, NotifiesOnlyOnChange) {
  auto capability = Create();
  capability->RefreshForTesting();
  EXPECT_TRUE(capability->can_actuate_in_background());
  EXPECT_EQ(changed_count_, 1);

  // Refreshing without a change doesn't notify.
  capability->RefreshForTesting();
  EXPECT_EQ(changed_count_, 1);

  SetCanActuateInBackground(false);
  capability->RefreshForTesting();
  EXPECT_FALSE(capability->can_actuate_in_background());
  EXPECT_EQ(changed_count_, 2);
  EXPECT_FALSE(GetPersistedValue());
}

TEST_F(BackgroundActuationCapabilityTest, RereadsWhenForegrounded) {
  auto capability = Create();
  // Let the read posted at construction finish first. Otherwise the foreground
  // event would coalesce into that pending read.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return capability->can_actuate_in_background(); }));

  SetCanActuateInBackground(false);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  // The read is deferred, so nothing changes synchronously.
  EXPECT_TRUE(capability->can_actuate_in_background());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !capability->can_actuate_in_background(); }));
  EXPECT_FALSE(GetPersistedValue());
}

TEST_F(BackgroundActuationCapabilityTest, SkipsReadWhenSignedOut) {
  identity_test_env_.ClearPrimaryAccount();
  auto capability = Create();
  capability->RefreshForTesting();
  EXPECT_FALSE(capability->can_actuate_in_background());
  EXPECT_FALSE(GetPersistedValue());
  EXPECT_EQ(changed_count_, 0);
}

TEST_F(BackgroundActuationCapabilityTest, ReadsOnSignIn) {
  identity_test_env_.ClearPrimaryAccount();
  auto capability = Create();
  capability->RefreshForTesting();
  ASSERT_FALSE(capability->can_actuate_in_background());

  // Signing in schedules a read without a foreground event.
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return capability->can_actuate_in_background(); }));
  EXPECT_TRUE(GetPersistedValue());
}

TEST_F(BackgroundActuationCapabilityTest,
       SkipsReadAfterIdentityManagerShutdown) {
  auto capability = Create();
  // Simulates the `IdentityManager` shutting down before this object.
  static_cast<signin::IdentityManager::Observer*>(capability.get())
      ->OnIdentityManagerShutdown(identity_test_env_.identity_manager());
  capability->RefreshForTesting();
  EXPECT_FALSE(capability->can_actuate_in_background());
  EXPECT_FALSE(GetPersistedValue());
}

TEST_F(BackgroundActuationCapabilityTest, DoesNothingWithNullPrefs) {
  BackgroundActuationCapability capability(
      /*prefs=*/nullptr, identity_test_env_.identity_manager(),
      base::DoNothing());
  EXPECT_FALSE(capability.can_actuate_in_background());
}

TEST_F(BackgroundActuationCapabilityTest, DoesNothingWithNullIdentityManager) {
  prefs_.SetBoolean(prefs::kGlicLastCanActuateInBackground, true);
  BackgroundActuationCapability capability(
      &prefs_, /*identity_manager=*/nullptr, base::DoNothing());
  EXPECT_FALSE(capability.can_actuate_in_background());
  EXPECT_TRUE(GetPersistedValue());
}

}  // namespace
}  // namespace glic
