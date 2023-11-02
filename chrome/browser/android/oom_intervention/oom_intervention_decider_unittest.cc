// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"

#include "base/strings/string_number_conversions.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockInterventionDeciderDelegate
    : public OomInterventionDecider::Delegate {
 public:
  explicit MockInterventionDeciderDelegate(bool was_clean)
      : was_clean_(was_clean) {}
  bool WasLastShutdownClean() override { return was_clean_; }

 private:
  bool was_clean_;
};

class OomInterventionDeciderTest : public testing::Test {
 protected:
  OomInterventionDeciderTest() {
    OomInterventionDecider::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(OomInterventionDeciderTest, OptOutSingleHost) {
  std::string host = "www.example.com";

  OomInterventionDecider decider(
      std::make_unique<MockInterventionDeciderDelegate>(true), &prefs_);
  decider.ClearData();

  EXPECT_TRUE(decider.CanTriggerIntervention(host));
  EXPECT_FALSE(decider.IsOptedOut(host));

  decider.OnInterventionDeclined(host);
  EXPECT_FALSE(decider.CanTriggerIntervention(host));
  EXPECT_FALSE(decider.IsOptedOut(host));

  decider.OnOomDetected(host);
  EXPECT_TRUE(decider.CanTriggerIntervention(host));
  EXPECT_FALSE(decider.IsOptedOut(host));

  decider.OnInterventionDeclined(host);
  EXPECT_FALSE(decider.CanTriggerIntervention(host));
  EXPECT_TRUE(decider.IsOptedOut(host));
}

TEST_F(OomInterventionDeciderTest, ParmanentlyOptOut) {
  OomInterventionDecider decider(
      std::make_unique<MockInterventionDeciderDelegate>(true), &prefs_);
  decider.ClearData();

  std::string not_declined_host = "not_declined_host";
  EXPECT_TRUE(decider.CanTriggerIntervention(not_declined_host));

  // Put sufficient number of hosts into the blocklist.
  for (size_t i = 0; i < OomInterventionDecider::kMaxBlocklistSize; ++i) {
    std::string declined_host = "declined_host" + base::NumberToString(i);
    decider.OnInterventionDeclined(declined_host);
    decider.OnOomDetected(declined_host);
    decider.OnInterventionDeclined(declined_host);
  }

  EXPECT_FALSE(decider.CanTriggerIntervention(not_declined_host));
  EXPECT_TRUE(decider.IsOptedOut(not_declined_host));
}

TEST_F(OomInterventionDeciderTest, WasLastShutdownClean) {
  std::string host = "www.example.com";

  {
    // Simulate a clean launch.
    OomInterventionDecider decider(
        std::make_unique<MockInterventionDeciderDelegate>(true), &prefs_);
    decider.ClearData();

    EXPECT_TRUE(decider.CanTriggerIntervention(host));

    decider.OnInterventionDeclined(host);
    EXPECT_FALSE(decider.CanTriggerIntervention(host));
    EXPECT_FALSE(decider.IsOptedOut(host));
  }

  {
    // Simulate a launch after a browser crash by passing a delegate which
    // returns false when WasLastShutdownClean() is called. |host| will be
    // considererd as a host which caused the crash (probably due to OOM).
    OomInterventionDecider decider(
        std::make_unique<MockInterventionDeciderDelegate>(false), &prefs_);

    EXPECT_TRUE(decider.CanTriggerIntervention(host));

    decider.OnInterventionDeclined(host);
    EXPECT_FALSE(decider.CanTriggerIntervention(host));
    EXPECT_TRUE(decider.IsOptedOut(host));
  }
}
