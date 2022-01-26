// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/cast_receiver/arc_cast_receiver_service.h"

#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_cast_receiver_instance.h"
#include "ash/components/settings/cros_settings_names.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace {

class ArcCastReceiverServiceTest : public testing::Test {
 protected:
  ArcCastReceiverServiceTest()
      // Reuse the settings service |profile_| automatically creates.
      : settings_helper_(/*create_settings_service=*/false) {}
  ArcCastReceiverServiceTest(const ArcCastReceiverServiceTest&) = delete;
  ArcCastReceiverServiceTest& operator=(const ArcCastReceiverServiceTest&) =
      delete;
  ~ArcCastReceiverServiceTest() override = default;

  void SetUp() override {
    // Initialize prefs and settings.
    prefs()->SetBoolean(prefs::kCastReceiverEnabled, false);
    settings_helper()->ReplaceDeviceSettingsProviderWithStub();
    settings_helper()->SetString(ash::kCastReceiverName, std::string());

    bridge_ = ArcCastReceiverService::GetForBrowserContextForTesting(&profile_);
    // This results in ArcCastReceiverService::OnInstanceReady being called.
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->cast_receiver()
        ->SetInstance(&cast_receiver_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->cast_receiver());
  }

  ArcCastReceiverService* bridge() { return bridge_; }
  PrefService* prefs() { return profile_.GetPrefs(); }
  ash::ScopedCrosSettingsTestHelper* settings_helper() {
    return &settings_helper_;
  }
  const FakeCastReceiverInstance* cast_receiver_instance() const {
    return &cast_receiver_instance_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeCastReceiverInstance cast_receiver_instance_;
  TestingProfile profile_;
  ash::ScopedCrosSettingsTestHelper settings_helper_;
  ArcCastReceiverService* bridge_ = nullptr;
};

TEST_F(ArcCastReceiverServiceTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

// Test that OnConnectionReady has already been called because of the
// SetInstance() call in SetUp().
TEST_F(ArcCastReceiverServiceTest, OnConnectionReady) {
  const absl::optional<bool>& last_enabled =
      cast_receiver_instance()->last_enabled();
  ASSERT_TRUE(last_enabled);    // SetEnabled() has already been called.
  EXPECT_FALSE(*last_enabled);  // ..and it is called with false.

  const absl::optional<std::string>& last_name =
      cast_receiver_instance()->last_name();
  EXPECT_FALSE(last_name);  // SetName() hasn't been called yet.
}

// Tests that updating prefs::kCastReceiverEnabled triggers the mojo call.
TEST_F(ArcCastReceiverServiceTest, OnCastReceiverEnabledChanged) {
  prefs()->SetBoolean(prefs::kCastReceiverEnabled, true);

  const absl::optional<bool>& last_enabled =
      cast_receiver_instance()->last_enabled();
  // Verify that the call was made with true.
  ASSERT_TRUE(last_enabled);
  EXPECT_TRUE(*last_enabled);
}

// Tests that updating ash::kCastReceiverName triggers the mojo call.
TEST_F(ArcCastReceiverServiceTest, OnCastReceiverNameChanged) {
  settings_helper()->SetString(ash::kCastReceiverName, "name");

  const absl::optional<std::string>& last_name =
      cast_receiver_instance()->last_name();
  // Verify that the call was made with "name".
  ASSERT_TRUE(last_name);
  EXPECT_EQ("name", *last_name);
}

}  // namespace
}  // namespace arc
