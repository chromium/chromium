// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace bluetooth {

namespace {

constexpr char kTestGooglerEmail[] = "user@google.com";
constexpr char kTestNonGooglerEmail[] = "user@gmail.com";

}  // namespace

class DebugLogsManagerTest : public testing::Test {
 public:
  DebugLogsManagerTest() = default;

  void SetUp() override { DebugLogsManager::RegisterPrefs(prefs_.registry()); }

  void TearDown() override { debug_logs_manager_.reset(); }

  void InitDebugManager(const char* email, bool debug_flag_enabled) {
    feature_list_.InitWithFeatureState(
        chromeos::features::kShowBluetoothDebugLogToggle, debug_flag_enabled);

    debug_logs_manager_ = std::make_unique<DebugLogsManager>(email, &prefs_);
  }

  void DeleteAndRecreateDebugManager(const char* email) {
    debug_logs_manager_.reset();
    debug_logs_manager_ = std::make_unique<DebugLogsManager>(email, &prefs_);
  }

  DebugLogsManager* debug_manager() const { return debug_logs_manager_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DebugLogsManager> debug_logs_manager_;
  TestingPrefServiceSimple prefs_;

  DISALLOW_COPY_AND_ASSIGN(DebugLogsManagerTest);
};

TEST_F(DebugLogsManagerTest, FlagNotEnabled) {
  InitDebugManager(kTestGooglerEmail, false /* debug_flag_enabled */);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, NonGoogler) {
  InitDebugManager(kTestNonGooglerEmail, true /* debug_flag_enabled */);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, ChangeDebugLogsState) {
  InitDebugManager(kTestGooglerEmail, true /* debug_flag_enabled */);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);

  debug_manager()->ChangeDebugLogsState(true);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);

  // debug logs state should be saved despite DebugLogsManager is destroyed.
  DeleteAndRecreateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);

  debug_manager()->ChangeDebugLogsState(false);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);

  DeleteAndRecreateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);
}

}  // namespace bluetooth

}  // namespace chromeos
