// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_demo_mode_preference_handler.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcDemoModePreferenceHandlerTest : public testing::Test {
 public:
  ArcDemoModePreferenceHandlerTest() = default;

  void SetUp() override {
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kDemoModeConfig,
        static_cast<int>(ash::DemoSession::DemoModeConfig::kNone));
    handler_ = base::WrapUnique<ArcDemoModePreferenceHandler>(
        new ArcDemoModePreferenceHandler(
            base::BindOnce(
                &ArcDemoModePreferenceHandlerTest::OnDemoPreferenceChanged,
                base::Unretained(this)),
            &pref_service_));
  }

  void TearDown() override { handler_.reset(); }

 protected:
  void SetArcVmEnabled() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv({"", "--enable-arcvm"});
  }

  bool handler_was_called() { return handler_was_called_; }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  void OnDemoPreferenceChanged() { handler_was_called_ = true; }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  bool handler_was_called_ = false;
  std::unique_ptr<ArcDemoModePreferenceHandler> handler_;
};

TEST_F(ArcDemoModePreferenceHandlerTest, ConstructDestruct) {}

TEST_F(ArcDemoModePreferenceHandlerTest, DemoPrefOnline) {
  SetArcVmEnabled();
  EXPECT_FALSE(handler_was_called());

  pref_service()->SetInteger(
      prefs::kDemoModeConfig,
      static_cast<int>(ash::DemoSession::DemoModeConfig::kOnline));

  EXPECT_TRUE(handler_was_called());
}

TEST_F(ArcDemoModePreferenceHandlerTest, DemoPrefOnline_NoArcVm) {
  EXPECT_FALSE(handler_was_called());

  pref_service()->SetInteger(
      prefs::kDemoModeConfig,
      static_cast<int>(ash::DemoSession::DemoModeConfig::kOnline));

  EXPECT_FALSE(handler_was_called());
}

TEST_F(ArcDemoModePreferenceHandlerTest, DemoPrefNone) {
  SetArcVmEnabled();
  EXPECT_FALSE(handler_was_called());

  pref_service()->SetInteger(
      prefs::kDemoModeConfig,
      static_cast<int>(ash::DemoSession::DemoModeConfig::kNone));

  EXPECT_FALSE(handler_was_called());
}

}  // namespace arc
