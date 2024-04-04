// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace {
using ::testing::IsNull;
}  // namespace

namespace ash {

class MahiManagerImplFeatureKeyTest : public testing::Test {
 public:
  MahiManagerImplFeatureKeyTest() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(ash::switches::kMahiFeatureKey, "hello");
  }

 protected:
  views::Widget* GetMahiPanelWidget() {
    return mahi_manager_impl_.mahi_panel_widget_.get();
  }

  MahiManagerImpl mahi_manager_impl_;

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
};

TEST_F(MahiManagerImplFeatureKeyTest, DoesNotShowWidgetIfFeatureKeyIsWrong) {
  mahi_manager_impl_.OpenMahiPanel(/*display_id=*/0);

  EXPECT_THAT(GetMahiPanelWidget(), IsNull());
}

}  // namespace ash
