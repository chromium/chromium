// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/system_toast_style.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr NudgeCatalogName kTestCatalogName =
    NudgeCatalogName::kTestCatalogName;

constexpr char kNudgeShownCount[] = "Ash.NotifierFramework.Nudge.ShownCount";
constexpr char kNudgeTimeToActionWithin1m[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
constexpr char kNudgeTimeToActionWithin1h[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";
constexpr char kNudgeTimeToActionWithinSession[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.WithinSession";

void SetLockedState(bool locked) {
  SessionInfo info;
  info.state = locked ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

}  // namespace

class AnchoredNudgeManagerImplTest : public AshTestBase {
 public:
  AnchoredNudgeManagerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kSystemNudgeV2);
  }

  AnchoredNudgeManagerImpl* anchored_nudge_manager() {
    return Shell::Get()->anchored_nudge_manager();
  }

  // Creates an `AnchoredNudgeData` object with only the required elements.
  AnchoredNudgeData CreateBaseNudgeData(
      const std::string& id,
      views::View* anchor_view,
      const std::u16string& body_text = std::u16string()) {
    return AnchoredNudgeData(id, kTestCatalogName, body_text, anchor_view);
  }

  void CancelNudge(const std::string& id) {
    anchored_nudge_manager()->Cancel(id);
  }

  std::map<std::string, raw_ptr<AnchoredNudge>> GetShownNudges() {
    return anchored_nudge_manager()->shown_nudges_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a nudge can be shown and its contents are properly sent.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_SingleNudge) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  const std::u16string text = u"text";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view, text);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Ensure the nudge is visible and has set the provided contents.
  raw_ptr<AnchoredNudge, DanglingUntriaged> nudge =
      raw_ptr<AnchoredNudge, DanglingUntriaged>(GetShownNudges()[id]);
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());
  EXPECT_EQ(text, nudge->GetBodyText());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Ensure the nudge widget was not activated when shown.
  EXPECT_FALSE(nudge->GetWidget()->IsActive());

  // Cancel the nudge, expect it to be removed from the shown nudges map.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that two nudges can be shown on screen at the same time.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_TwoNudges) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  const std::string id_2 = "id_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data_2 = CreateBaseNudgeData(id_2, anchor_view_2);

  // Show the first nudge, expect the first nudge shown.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);

  // Show the second nudge, expect both nudges shown.
  anchored_nudge_manager()->Show(nudge_data_2);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_TRUE(GetShownNudges()[id_2]);

  // Cancel the second nudge, expect the first nudge shown.
  CancelNudge(id_2);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);

  // Cancel the first nudge, expect no nudges shown.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);
}

// Tests that a nudge with buttons can be shown, execute callbacks and dismiss
// the nudge when the button is pressed.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_WithButtons) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  const std::u16string dismiss_text = u"dismiss";
  const std::u16string second_button_text = u"second";
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Add a dismiss button with no callbacks.
  nudge_data.dismiss_text = dismiss_text;

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Ensure the nudge is visible and has set the provided contents.
  raw_ptr<AnchoredNudge, DanglingUntriaged> nudge =
      raw_ptr<AnchoredNudge, DanglingUntriaged>(GetShownNudges()[id]);
  EXPECT_TRUE(nudge);
  EXPECT_EQ(dismiss_text, nudge->GetDismissButton()->GetText());

  // Ensure the nudge does not have a second button.
  EXPECT_FALSE(nudge->GetSecondButton());

  // Press the dismiss button, the nudge should have dismissed.
  LeftClickOn(nudge->GetDismissButton());
  EXPECT_FALSE(GetShownNudges()[id]);

  // Show the nudge again.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Add callbacks for the dismiss button.
  bool dismiss_button_callback_ran = false;
  nudge_data.dismiss_callback = base::BindLambdaForTesting(
      [&dismiss_button_callback_ran] { dismiss_button_callback_ran = true; });

  // Show the nudge again.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Press the dismiss button, `dismiss_button_callback` should have executed,
  // and the nudge should have dismissed.
  LeftClickOn(nudge->GetDismissButton());
  EXPECT_TRUE(dismiss_button_callback_ran);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Add a second button with no callbacks.
  nudge_data.second_button_text = second_button_text;

  // Show the nudge again, now with a second button.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Ensure the nudge has a second button.
  EXPECT_TRUE(nudge->GetSecondButton());

  // Press the second button, the nudge should have dismissed.
  LeftClickOn(nudge->GetSecondButton());
  EXPECT_FALSE(GetShownNudges()[id]);

  // Add a callback for the second button.
  bool second_button_callback_ran = false;
  nudge_data.second_button_callback = base::BindLambdaForTesting(
      [&second_button_callback_ran] { second_button_callback_ran = true; });

  // Show the nudge again.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Press the second button, `second_button_callback` should have executed, and
  // the nudge should have dismissed.
  LeftClickOn(nudge->GetSecondButton());
  EXPECT_TRUE(second_button_callback_ran);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that attempting to show a nudge with an `id` that's in use cancels
// the existing nudge and replaces it with a new nudge.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_NudgeWithIdAlreadyExists) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up nudge data contents.
  const std::string id = "id";

  const std::u16string text = u"text";
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view, text);

  const std::u16string text_2 = u"text_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data_2 = CreateBaseNudgeData(id, anchor_view_2, text_2);

  // Show a nudge with some initial contents.
  anchored_nudge_manager()->Show(nudge_data);
  raw_ptr<AnchoredNudge, DanglingUntriaged> nudge =
      raw_ptr<AnchoredNudge, DanglingUntriaged>(GetShownNudges()[id]);
  EXPECT_EQ(text, nudge->GetBodyText());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Attempt to show a nudge with different contents but with the same id.
  anchored_nudge_manager()->Show(nudge_data_2);

  // Previously shown nudge should be cancelled and replaced with new nudge.
  nudge = GetShownNudges()[id];
  EXPECT_EQ(text_2, nudge->GetBodyText());
  EXPECT_EQ(anchor_view_2, nudge->GetAnchorView());

  // Cleanup.
  CancelNudge(id);
}

// Tests that a nudge is not created if its anchor view is not visible.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_InvisibleAnchorView) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Set anchor view visibility to false.
  anchor_view->SetVisible(false);

  // Attempt to show nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Anchor view is not visible, the nudge should not be created.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge is not created if its anchor view doesn't have a widget.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_AnchorViewWithoutWidget) {
  // Set up nudge data contents.
  const std::string id = "id";
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Attempt to show nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Anchor view does not have a widget, the nudge should not be created.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge closes if its anchor view is made invisible.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsHiding) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Set the anchor view visibility to false, the nudge should have closed.
  anchor_view->SetVisible(false);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Set the anchor view visibility to true, the nudge should not reappear.
  anchor_view->SetVisible(true);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge closes if its anchor view is deleted.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsDeleting) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";

  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Delete the anchor view, the nudge should have closed.
  contents_view->RemoveAllChildViews();
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge whose anchor view is a part of a secondary display closes
// when that display is removed.
TEST_F(AnchoredNudgeManagerImplTest,
       NudgeCloses_WhenAnchorViewIsDeletingOnSecondaryDisplay) {
  // Set up two displays.
  UpdateDisplay("800x700,800x700");
  RootWindowController* const secondary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id());

  // Set up nudge data contents. The anchor view is a child of the secondary
  // root window controller, so it will be deleted if the display is removed.
  const std::string id = "id";
  auto* anchor_view = secondary_root_window_controller->shelf()
                          ->status_area_widget()
                          ->unified_system_tray();
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show the nudge in the secondary display.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Remove the secondary display, which deletes the anchor view.
  UpdateDisplay("800x700");

  // The anchor view was deleted, the nudge should have closed.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge is properly destroyed on shutdown.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnShutdown) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Nudge is left open, no crash.
}

// Tests that nudges with `has_infinite_duration` set to false expire after
// their default duration reaches its end.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenDismissTimerExpires) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Fast forward `kNudgeDefaultDuration` plus one second, the nudge should have
  // expired.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kAnchoredNudgeDuration + base::Seconds(1));
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that nudges are destroyed on session state changes.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnSessionStateChanged) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Lock screen, nudge should have closed.
  SetLockedState(true);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Show a nudge in the locked state.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Unlock screen, nudge should have closed.
  SetLockedState(false);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that nudges with `has_infinite_duration` set to true will not expire
// after the default duration time has passed.
TEST_F(AnchoredNudgeManagerImplTest, NudgeWithInfiniteDuration) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);
  nudge_data.has_infinite_duration = true;

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Fast forward `kNudgeDefaultDuration` plus one second, the nudge should not
  // have expired.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kAnchoredNudgeDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudges()[id]);

  // Nudge with infinite duration is left open, no crash on shutdown.
}

// Tests that attempting to cancel a nudge with an invalid `id` should not
// have any effects.
TEST_F(AnchoredNudgeManagerImplTest, CancelNudgeWhichDoesNotExist) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  const std::string id_2 = "id_2";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Attempt to cancel nudge with an `id` that does not exist. Should not have
  // any effect.
  CancelNudge(id_2);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Cancel the shown nudge with its valid `id`.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Attempt to cancel the same nudge again. Should not have any effect.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);
}

TEST_F(AnchoredNudgeManagerImplTest, ShownCountMetric) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 0);

  anchored_nudge_manager()->Show(nudge_data);
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 1);

  anchored_nudge_manager()->Show(nudge_data);
  anchored_nudge_manager()->Show(nudge_data);
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 3);
}

TEST_F(AnchoredNudgeManagerImplTest, TimeToActionMetric) {
  base::HistogramTester histogram_tester;
  anchored_nudge_manager()->ResetNudgeRegistryForTesting();
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Metric is not recorded if the nudge has not been shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 0);

  // Metric is recorded if the action is performed after the nudge was shown.
  anchored_nudge_manager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Seconds(1));
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is recorded with the appropriate time range after showing nudge
  // again and waiting enough time to fall into the "Within1h" time bucket.
  anchored_nudge_manager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Minutes(2));
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  // Metric is recorded with the appropriate time range after showing nudge
  // again and waiting enough time to fall into the "WithinSession" time bucket.
  anchored_nudge_manager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Hours(2));
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);
}

}  // namespace ash
