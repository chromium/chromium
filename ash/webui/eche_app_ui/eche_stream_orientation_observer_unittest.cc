// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_stream_orientation_observer.h"

#include "ash/constants/ash_features.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::eche_app {

class EcheStreamOrientationObserverTest : public AshTestBase {
 public:
  EcheStreamOrientationObserverTest() = default;
  EcheStreamOrientationObserverTest(const EcheStreamOrientationObserverTest&) =
      delete;
  EcheStreamOrientationObserverTest& operator=(
      const EcheStreamOrientationObserverTest&) = delete;
  ~EcheStreamOrientationObserverTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    DCHECK(test_web_view_factory_.get());
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    eche_tray_ =
        ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
    observer_ = std::make_unique<EcheStreamOrientationObserver>();
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  void OnStreamOrientationChanged(bool is_landscape) {
    observer_->OnStreamOrientationChanged(is_landscape);
  }

  EcheTray* eche_tray() { return eche_tray_; }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<EcheTray, DanglingUntriaged> eche_tray_ = nullptr;
  std::unique_ptr<EcheStreamOrientationObserver> observer_;
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheStreamOrientationObserverTest, OnStreamOrientationChanged) {
  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);

  OnStreamOrientationChanged(true);

  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), true);

  OnStreamOrientationChanged(false);

  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);

  OnStreamOrientationChanged(false);

  // Should not change
  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);
}

TEST_F(EcheStreamOrientationObserverTest, EcheSWAFlagDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kEcheSWA});

  EXPECT_FALSE(eche_tray()->IsInitialized());
  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);

  OnStreamOrientationChanged(false);

  EXPECT_FALSE(eche_tray()->IsInitialized());
  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);
}

}  // namespace ash::eche_app
