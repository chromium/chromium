// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/system_ui/arc_system_ui_bridge.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_system_ui_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "base/logging.h"
#include "base/test/mock_log.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
namespace arc {

#define EXPECT_ERROR_LOG(matcher)                                \
  if (DLOG_IS_ON(ERROR)) {                                       \
    EXPECT_CALL(log_, Log(logging::LOG_ERROR, _, _, _, matcher)) \
        .WillOnce(testing::Return(true)); /* suppress logging */ \
  }

class ArcSystemUIBridgeTest : public testing::Test {
 protected:
  ArcSystemUIBridgeTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        bridge_(ArcSystemUIBridge::GetForBrowserContextForTesting(&context_)) {
    ArcServiceManager::Get()->arc_bridge_service()->system_ui()->SetInstance(
        &system_ui_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->system_ui());

    // ARC has VLOG(1) enabled. Ignore and suppress these logs if the test
    // will verify log output. Note the "if" must match the "if" in
    // `EXPECT_ERROR_LOG`.
    if (DLOG_IS_ON(ERROR)) {
      EXPECT_CALL(log_, Log(-1, _, _, _, _))
          .WillRepeatedly(testing::Return(true));
    }
  }

  ~ArcSystemUIBridgeTest() override {
    ArcServiceManager::Get()->arc_bridge_service()->system_ui()->CloseInstance(
        &system_ui_instance_);
    bridge_->Shutdown();
  }

  explicit ArcSystemUIBridgeTest(const ArcSystemUIBridge&) = delete;
  ArcSystemUIBridgeTest& operator=(const ArcSystemUIBridge&) = delete;

  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  FakeSystemUiInstance system_ui_instance_;
  ArcSystemUIBridge* const bridge_;
  base::test::MockLog log_;
};

TEST_F(ArcSystemUIBridgeTest, ConstructDestruct) {}

TEST_F(ArcSystemUIBridgeTest, OnColorModeChanged) {
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  bridge_->OnColorModeChanged(true);
  EXPECT_TRUE(system_ui_instance_.dark_theme_status());
  ArcServiceManager::Get()->arc_bridge_service()->system_ui()->CloseInstance(
      &system_ui_instance_);
  EXPECT_ERROR_LOG(testing::HasSubstr("OnColorModeChanged failed"));
  log_.StartCapturingLogs();
  bridge_->OnColorModeChanged(true);
}

TEST_F(ArcSystemUIBridgeTest, OnConnectionReady) {
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  bridge_->OnColorModeChanged(true);
  EXPECT_TRUE(system_ui_instance_.dark_theme_status());
  bridge_->OnConnectionReady();
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  ArcServiceManager::Get()->arc_bridge_service()->system_ui()->CloseInstance(
      &system_ui_instance_);
  EXPECT_ERROR_LOG(testing::HasSubstr("OnConnectionReady failed"));
  log_.StartCapturingLogs();
  bridge_->OnConnectionReady();
}

TEST_F(ArcSystemUIBridgeTest, SendOverlayColor) {
  EXPECT_EQ((uint32_t)0, system_ui_instance_.source_color());
  EXPECT_EQ(mojom::ThemeStyleType::TONAL_SPOT,
            system_ui_instance_.theme_style());
  bridge_->SendOverlayColor(50, mojom::ThemeStyleType::EXPRESSIVE);
  EXPECT_EQ((uint32_t)50, system_ui_instance_.source_color());
  EXPECT_EQ(mojom::ThemeStyleType::EXPRESSIVE,
            system_ui_instance_.theme_style());
}

}  // namespace arc
