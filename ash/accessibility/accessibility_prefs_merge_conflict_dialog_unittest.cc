// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_prefs_merge_conflict_dialog.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_prefs_merge_conflict_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/style/switch.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

namespace {

template <typename T>
void CollectChildrenOfType(views::View* root, std::vector<T*>& out) {
  if (auto* casted = views::AsViewClass<T>(root)) {
    out.push_back(casted);
  }
  for (views::View* child : root->children()) {
    CollectChildrenOfType<T>(child, out);
  }
}

template <typename T>
std::vector<T*> GetAllChildrenOfType(views::View* root) {
  std::vector<T*> result;
  CollectChildrenOfType<T>(root, result);
  return result;
}

using PrefConflict = AccessibilityPrefsMergeConflictController::PrefConflict;

class FakeController : public AccessibilityPrefsMergeConflictController {
 public:
  explicit FakeController(std::vector<PrefConflict> conflicts)
      : AccessibilityPrefsMergeConflictController(std::move(conflicts)) {}

  void UpdateConflict(std::string_view pref_name, base::Value value) override {
    last_updated_pref = pref_name;
    last_updated_value = std::move(value);
  }

  std::string last_updated_pref;
  base::Value last_updated_value;
};

class MockCallback {
 public:
  MOCK_METHOD(void, Callback, ());
};

}  // namespace

class AccessibilityPrefsMergeConflictDialogTest : public AshTestBase {
 public:
  AccessibilityPrefsMergeConflictDialog* ShowDialog(
      std::unique_ptr<FakeController> controller) {
    dialog_widget_ = AccessibilityPrefsMergeConflictDialog::CreateAndShow(
        std::move(controller),
        base::BindOnce(&MockCallback::Callback,
                       base::Unretained(&on_closing_callback_)));

    return static_cast<AccessibilityPrefsMergeConflictDialog*>(
        dialog_widget_->widget_delegate());
  }

 protected:
  MockCallback on_closing_callback_;

 private:
  views::UniqueWidgetPtr dialog_widget_;
};

TEST_F(AccessibilityPrefsMergeConflictDialogTest, BuildsRowsFromConflicts) {
  std::vector<FakeController::PrefConflict> conflicts;
  conflicts.emplace_back(prefs::kAccessibilityHighContrastEnabled,
                         base::Value(true), base::Value(false));

  auto controller = std::make_unique<FakeController>(std::move(conflicts));
  auto* dialog = ShowDialog(std::move(controller));

  EXPECT_EQ(GetAllChildrenOfType<HoverHighlightView>(dialog).size(), 1u);
}

TEST_F(AccessibilityPrefsMergeConflictDialogTest, ToggleReflectsInitialState) {
  std::vector<FakeController::PrefConflict> conflicts;
  conflicts.emplace_back(prefs::kAccessibilityHighContrastEnabled,
                         base::Value(true), base::Value(false));

  auto controller = std::make_unique<FakeController>(std::move(conflicts));
  auto* dialog = ShowDialog(std::move(controller));

  auto* item = GetAllChildrenOfType<HoverHighlightView>(dialog)[0];
  auto* toggle = views::AsViewClass<Switch>(item->right_view());

  EXPECT_TRUE(toggle->GetIsOn());
}

TEST_F(AccessibilityPrefsMergeConflictDialogTest,
       ClickingRowTogglesAndUpdatesController) {
  std::vector<FakeController::PrefConflict> conflicts;
  conflicts.emplace_back(prefs::kAccessibilityHighContrastEnabled,
                         base::Value(true), base::Value(false));

  auto controller = std::make_unique<FakeController>(std::move(conflicts));
  auto* controller_ptr = controller.get();

  auto* dialog = ShowDialog(std::move(controller));

  auto* item = GetAllChildrenOfType<HoverHighlightView>(dialog)[0];

  // Toggle.
  LeftClickOn(item);
  auto* toggle = views::AsViewClass<Switch>(item->right_view());

  EXPECT_FALSE(toggle->GetIsOn());
  EXPECT_EQ(controller_ptr->last_updated_pref,
            prefs::kAccessibilityHighContrastEnabled);
}

TEST_F(AccessibilityPrefsMergeConflictDialogTest,
       AccessibilityStateStaysInSyncWithToggle) {
  std::vector<FakeController::PrefConflict> conflicts;
  conflicts.emplace_back(prefs::kAccessibilityHighContrastEnabled,
                         base::Value(true), base::Value(false));

  auto controller = std::make_unique<FakeController>(std::move(conflicts));
  auto* dialog = ShowDialog(std::move(controller));

  auto* item = GetAllChildrenOfType<HoverHighlightView>(dialog)[0];

  ui::AXNodeData data;

  // Initial state.
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  // Toggle.
  LeftClickOn(item);

  // Final state.
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
}

TEST_F(AccessibilityPrefsMergeConflictDialogTest,
       ClickingAcceptButtonCallsCloseCallback) {
  EXPECT_CALL(on_closing_callback_, Callback()).Times(1);

  std::vector<FakeController::PrefConflict> conflicts;
  conflicts.emplace_back(prefs::kAccessibilityHighContrastEnabled,
                         base::Value(true), base::Value(false));

  auto controller = std::make_unique<FakeController>(std::move(conflicts));
  auto* dialog = ShowDialog(std::move(controller));

  LeftClickOn(dialog->GetAcceptButtonForTesting());
}

TEST_F(AccessibilityPrefsMergeConflictDialogTest,
       ClickingGoSettingsButtonCallsCloseCallback) {
  EXPECT_CALL(on_closing_callback_, Callback()).Times(1);

  std::vector<FakeController::PrefConflict> conflicts;
  conflicts.emplace_back(prefs::kAccessibilityHighContrastEnabled,
                         base::Value(true), base::Value(false));

  auto controller = std::make_unique<FakeController>(std::move(conflicts));
  auto* dialog = ShowDialog(std::move(controller));

  LeftClickOn(dialog->GetCancelButtonForTesting());
}

}  // namespace ash
