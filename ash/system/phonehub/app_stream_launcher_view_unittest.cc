// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_view.h"
#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"

namespace ash {

class AppStreamLauncherViewTest : public AshTestBase {
 public:
  AppStreamLauncherViewTest() = default;
  ~AppStreamLauncherViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA},
        /*disabled_features=*/{});

    app_stream_launcher_view_ =
        std::make_unique<AppStreamLauncherView>(&fake_phone_hub_manager_);
  }

  // AshTestBase:
  void TearDown() override {
    app_stream_launcher_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  AppStreamLauncherView* app_stream_launcher_view() {
    return app_stream_launcher_view_.get();
  }
  phonehub::FakePhoneHubManager* fake_phone_hub_manager() {
    return &fake_phone_hub_manager_;
  }

 private:
  std::unique_ptr<AppStreamLauncherView> app_stream_launcher_view_;
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AppStreamLauncherViewTest, OpenView) {
  EXPECT_TRUE(app_stream_launcher_view()->GetVisible());
}

}  // namespace ash
