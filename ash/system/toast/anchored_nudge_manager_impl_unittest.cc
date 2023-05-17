// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/system_toast_style.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AnchoredNudgeManagerImplTest : public AshTestBase {
 public:
  AnchoredNudgeManagerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  AnchoredNudgeManagerImplTest(const AnchoredNudgeManagerImplTest&) = delete;
  AnchoredNudgeManagerImplTest& operator=(const AnchoredNudgeManagerImplTest&) =
      delete;
  ~AnchoredNudgeManagerImplTest() override = default;

  AnchoredNudgeManagerImpl* anchored_nudge_manager() {
    return Shell::Get()->anchored_nudge_manager();
  }

  void ShowNudge(const std::string& id,
                 views::View* anchor_view,
                 const std::u16string& text = std::u16string(),
                 bool has_infinite_duration = false) {
    AnchoredNudgeData nudge_data(id, AnchoredNudgeCatalogName::kTest, text,
                                 anchor_view);
    nudge_data.has_infinite_duration = has_infinite_duration;

    anchored_nudge_manager()->Show(nudge_data);
  }

  void CancelNudge(const std::string& id) {
    anchored_nudge_manager()->Cancel(id);
  }

  std::map<std::string, raw_ptr<AnchoredNudge>> GetShownNudges() {
    return anchored_nudge_manager()->shown_nudges_;
  }
};

// Tests that a nudge can be shown and its contents are properly sent.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_SingleNudge) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  const std::u16string text = u"text";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());

  // Show a nudge.
  ShowNudge(id, anchor_view, text);

  // Ensure the nudge is visible and has set the provided contents.
  auto nudge = GetShownNudges()[id];
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());
  EXPECT_EQ(text, nudge->GetText());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

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

  const std::string id_2 = "id_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());

  // Show the first nudge, expect the first nudge shown.
  ShowNudge(id, anchor_view);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);

  // Show the second nudge, expect both nudges shown.
  ShowNudge(id_2, anchor_view_2);
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

  const std::u16string text_2 = u"text_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());

  // Show a nudge with some initial contents.
  ShowNudge(id, anchor_view, text);
  auto nudge = GetShownNudges()[id];
  EXPECT_EQ(text, nudge->GetText());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Attempt to show a nudge with different contents but with the same id.
  ShowNudge(id, anchor_view_2, text_2);

  // Previously shown nudge should be cancelled and replaced with new nudge.
  nudge = GetShownNudges()[id];
  EXPECT_EQ(text_2, nudge->GetText());
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

  // Set anchor view visibility to false.
  anchor_view->SetVisible(false);

  // Attempt to show nudge.
  ShowNudge(id, anchor_view);

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

  // Attempt to show nudge.
  ShowNudge(id, anchor_view);

  // Anchor view does not have a widget, the nudge should not be created.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge closes if its anchor view is made invisible.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsHiding) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());

  // Show a nudge.
  ShowNudge(id, anchor_view);
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

  // Show a nudge.
  ShowNudge(id, anchor_view);
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

  // Set up nudge data contents.
  const std::string id = "id";
  // The anchor view is a child of the secondary root window controller, so it
  // will be deleted if the display is removed.
  auto* secondary_unified_system_tray =
      secondary_root_window_controller->shelf()
          ->status_area_widget()
          ->unified_system_tray();

  // Show the nudge in the secondary display.
  ShowNudge(id, secondary_unified_system_tray);
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

  // Show a nudge.
  ShowNudge(id, anchor_view);
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

  // Show a nudge.
  ShowNudge(id, anchor_view);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Fast forward `kNudgeDefaultDuration` plus one second, the nudge should have
  // expired.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kAnchoredNudgeDuration + base::Seconds(1));
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that nudges with `has_infinite_duration` set to true will not expire
// after the default duration time has passed.
TEST_F(AnchoredNudgeManagerImplTest, NudgeWithInfiniteDuration) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  const std::u16string text = u"text";
  bool has_infinite_duration = true;

  // Show a nudge.
  ShowNudge(id, anchor_view, text, has_infinite_duration);
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

  // Show a nudge.
  ShowNudge(id, anchor_view);
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

}  // namespace ash
