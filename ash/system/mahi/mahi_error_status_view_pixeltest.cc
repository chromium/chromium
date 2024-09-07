// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Aliases
using chromeos::MahiResponseStatus;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Values;

std::string GetScreenShotNameForErrorStatus(MahiResponseStatus status) {
  switch (status) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
      return "CantFindOutputData";
    case chromeos::MahiResponseStatus::kContentExtractionError:
      return "ContentExtractionError";
    case chromeos::MahiResponseStatus::kInappropriate:
      return "Inappropriate";
    case chromeos::MahiResponseStatus::kUnknownError:
      return "UnknownError";
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
      return "QuotaLimitHit";
    case chromeos::MahiResponseStatus::kResourceExhausted:
      return "ResourceExhausted";
    case chromeos::MahiResponseStatus::kRestrictedCountry:
      return "RestrictedCountry";
    case chromeos::MahiResponseStatus::kUnsupportedLanguage:
      return "UnsupportedLanguage";
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      NOTREACHED();
  }
}

}  // namespace

class MahiErrorStatusViewPixelTestBase : public AshTestBase {
 protected:
  void ShowMahiPanel() {
    mahi_panel_widget_ = MahiPanelWidget::CreateAndShowPanelWidget(
        GetPrimaryDisplay().id(), /*mahi_menu_bounds=*/gfx::Rect(),
        ui_controller());
    mahi_panel_widget_->Show();
  }

  views::Widget* mahi_panel_widget() { return mahi_panel_widget_.get(); }
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }
  MahiUiController* ui_controller() { return &ui_controller_; }

 private:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    AshTestBase::SetUp();

    ON_CALL(mock_mahi_manager_, GetContentTitle)
        .WillByDefault(Return(u"content title"));
  }

  void TearDown() override {
    mahi_panel_widget_.reset();

    AshTestBase::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  MahiUiController ui_controller_;
  chromeos::ScopedMahiManagerSetter scoped_setter_{&mock_mahi_manager_};
  views::UniqueWidgetPtr mahi_panel_widget_;
};

// MahiErrorStatusViewPixelTest ------------------------------------------------

class MahiErrorStatusViewPixelTest
    : public MahiErrorStatusViewPixelTestBase,
      public testing::WithParamInterface<MahiResponseStatus> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MahiErrorStatusViewPixelTest,
                         Values(MahiResponseStatus::kCantFindOutputData,
                                MahiResponseStatus::kContentExtractionError,
                                MahiResponseStatus::kInappropriate,
                                MahiResponseStatus::kQuotaLimitHit,
                                MahiResponseStatus::kResourceExhausted,
                                MahiResponseStatus::kRestrictedCountry,
                                MahiResponseStatus::kUnsupportedLanguage,
                                MahiResponseStatus::kUnknownError));

// Verifies the error status view when a summary update incurs an error
// specified by the test param.
TEST_P(MahiErrorStatusViewPixelTest, Basics) {
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(u"fake summary", GetParam());
      });

  ShowMahiPanel();
  views::View* const error_status_view =
      mahi_panel_widget()->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kErrorStatusView);
  ASSERT_TRUE(error_status_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenShotNameForErrorStatus(GetParam()), /*revision_number=*/5,
      error_status_view));
}

// Verifies the error status on the Mahi panel scroll view when asking a
// question.
TEST_P(MahiErrorStatusViewPixelTest, QuestionAnswerView) {
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(u"answer", GetParam());
          });

  ShowMahiPanel();
  views::View* const mahi_contents_view =
      mahi_panel_widget()->GetContentsView();
  auto* const question_textfield =
      views::AsViewClass<views::Textfield>(mahi_contents_view->GetViewByID(
          mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  question_textfield->SetText(u"fake inappropriate question");

  auto* const send_button = mahi_contents_view->GetViewByID(
      mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  LeftClickOn(send_button);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenShotNameForErrorStatus(GetParam()), /*revision_number=*/3,
      mahi_contents_view->GetViewByID(mahi_constants::ViewId::kScrollView)));
}

}  // namespace ash
