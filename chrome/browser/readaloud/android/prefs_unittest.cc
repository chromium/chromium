// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/prefs.h"

#include <memory>

#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class ReadAloudPrefsTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterProfilePrefs(pref_service_->registry());
  }

 protected:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};

TEST_F(ReadAloudPrefsTest, GetReliabilityLoggingId_NotAllowed) {
  // Return 0 if metrics_id is empty.
  EXPECT_EQ(0ULL, GetReliabilityLoggingId(*pref_service_, ""));
}

TEST_F(ReadAloudPrefsTest, GetReliabilityLoggingId_SameMetricsID) {
  uint64_t id = GetReliabilityLoggingId(*pref_service_, "abcd");
  EXPECT_NE(0ULL, id);

  // Second call with same metrics_id should return the same value.
  EXPECT_EQ(id, GetReliabilityLoggingId(*pref_service_, "abcd"));
}

TEST_F(ReadAloudPrefsTest, GetReliabilityLoggingId_DifferentMetricsID) {
  uint64_t id = GetReliabilityLoggingId(*pref_service_, "abcd");
  EXPECT_NE(0ULL, id);

  // Second call with different metrics_id should return a different value.
  EXPECT_NE(id, GetReliabilityLoggingId(*pref_service_, "efgh"));
}

}  // namespace readaloud
