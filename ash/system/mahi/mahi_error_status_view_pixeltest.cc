// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

namespace {

// Aliases
using chromeos::MahiResponseStatus;
using ::testing::NiceMock;
using ::testing::Return;

}  // namespace

class MahiErrorStatusViewPixelTest : public AshTestBase {
 protected:
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

 private:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kMahi);
    AshTestBase::SetUp();
    ON_CALL(mock_mahi_manager_, GetContentTitle)
        .WillByDefault(Return(u"content title"));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  chromeos::ScopedMahiManagerSetter scoped_setter_{&mock_mahi_manager_};
};

// Verifies the error status view when a summary update incurs an unknown error.
// TODO(http://b/332410573): Add pixel tests to cover all error states.
TEST_F(MahiErrorStatusViewPixelTest, Basics) {
  // Config the mock mahi manager to return a summary with an unknown error.
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(u"fake summary",
                                MahiResponseStatus::kUnknownError);
      });

  views::UniqueWidgetPtr mahi_panel_widget =
      MahiPanelWidget::CreatePanelWidget(GetPrimaryDisplay().id());
  mahi_panel_widget->Show();

  views::View* const error_status_view =
      mahi_panel_widget->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kErrorStatusView);
  ASSERT_TRUE(error_status_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "basics", /*revision_number=*/3, error_status_view));
}

}  // namespace ash
