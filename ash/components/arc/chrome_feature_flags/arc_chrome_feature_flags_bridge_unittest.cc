// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/chrome_feature_flags/arc_chrome_feature_flags_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_chrome_feature_flags_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcChromeFeatureFlagsBridgeTest : public testing::Test {
 protected:
  ArcChromeFeatureFlagsBridgeTest()
      : bridge_(ArcChromeFeatureFlagsBridge::GetForBrowserContextForTesting(
            &context_)) {}
  ArcChromeFeatureFlagsBridgeTest(const ArcChromeFeatureFlagsBridgeTest&) =
      delete;
  ArcChromeFeatureFlagsBridgeTest& operator=(
      const ArcChromeFeatureFlagsBridgeTest&) = delete;
  ~ArcChromeFeatureFlagsBridgeTest() override = default;

  void TearDown() override {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->chrome_feature_flags()
        ->CloseInstance(&instance_);
    bridge_->Shutdown();
  }

  void Connect() {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->chrome_feature_flags()
        ->SetInstance(&instance_);
  }

  ArcChromeFeatureFlagsBridge* bridge() { return bridge_; }
  FakeChromeFeatureFlagsInstance* instance() { return &instance_; }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  FakeChromeFeatureFlagsInstance instance_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const raw_ptr<ArcChromeFeatureFlagsBridge, ExperimentalAsh> bridge_;
};

TEST_F(ArcChromeFeatureFlagsBridgeTest, ConstructDestruct) {
  Connect();
  EXPECT_NE(nullptr, bridge());
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyQsRevamp_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(ash::features::kQsRevamp);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->qs_revamp);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyQsRevamp_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(ash::features::kQsRevamp);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->qs_revamp);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyJelly_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(chromeos::features::kJelly);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->jelly_colors);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyJelly_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(chromeos::features::kJelly);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->jelly_colors);
}

}  // namespace
}  // namespace arc
