// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/test/view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "components/ukm/test_ukm_recorder.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

class EditLabelTest : public OverlayViewTestBase {
 public:
  EditLabelTest() = default;

  ActionLabel* GetLabel(const ActionView* action_view, size_t index) const {
    auto& labels = action_view->labels();
    DCHECK_LT(index, labels.size());
    return labels[index];
  }

  void TapKeyboardKeyOnEditLabel(EditLabel* label, ui::KeyboardCode code) {
    label->OnKeyPressed(
        ui::KeyEvent(ui::EventType::kKeyPressed, code, ui::EF_NONE));
    label->OnKeyReleased(
        ui::KeyEvent(ui::EventType::kKeyReleased, code, ui::EF_NONE));
  }

  void FocusOnLabel(EditLabel* label) {
    DCHECK(label);
    label->OnFocus();
  }

  void BlurOnLabel(EditLabel* label) {
    DCHECK(label);
    label->OnBlur();
  }

  bool IsInErrorState(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    return IsNameTagInErrorState(menu->action_edit_->labels_view_);
  }

  bool IsInErrorState(ActionViewListItem* list_item) {
    DCHECK(list_item);
    return IsNameTagInErrorState(list_item->labels_view_);
  }

  void CheckAction(Action* action,
                   ActionType expect_action_type,
                   const std::vector<ui::DomCode>& expected_code,
                   const std::vector<std::u16string>& expected_labels,
                   const std::u16string expected_name) {
    DCHECK(action);
    ShowButtonOptionsMenu(action);

    EXPECT_EQ(expect_action_type, action->GetType());
    VerifyActionKeyBinding(action, expected_code);
    VerifyUIDisplay(action, expected_labels, expected_name);
  }

  void CheckErrorState(ButtonOptionsMenu* menu,
                       ActionViewListItem* list_item,
                       bool menu_has_error,
                       bool list_item_has_error) {
    EXPECT_EQ(menu_has_error, IsInErrorState(menu));
    EXPECT_EQ(list_item_has_error, IsInErrorState(list_item));
  }

  // Returns `ButtonOptionsMenu` if there is one shown. Otherwise, return
  // nullptr;
  ButtonOptionsMenu* GetButtonOptionsMenu() {
    return controller_->GetButtonOptionsMenu();
  }

  ActionViewListItem* GetActionViewListItem(Action* action) {
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    if (editing_list_->is_zero_state_) {
      return nullptr;
    }
    for (views::View* child : scroll_content->children()) {
      if (auto* list_item = views::AsViewClass<ActionViewListItem>(child);
          list_item->action() == action) {
        return list_item;
      }
    }
    return nullptr;
  }

  Action* GetAction(ButtonOptionsMenu* menu) {
    if (!menu) {
      return nullptr;
    }
    DCHECK(controller_);
    return menu->action();
  }

 private:
  // Checks if the name tag attached to `edit_labels` is in error state.
  bool IsNameTagInErrorState(EditLabels* edit_labels) {
    DCHECK(edit_labels);
    NameTag* name_tag = edit_labels->name_tag_;
    DCHECK(name_tag);
    views::ImageView* error_icon = name_tag->error_icon_;
    DCHECK(error_icon);
    return error_icon->GetVisible();
  }
};

TEST_F(EditLabelTest, TestEditingListLabelEditing) {
  // Modify the label for ActionTap and noting is conflicted.
  // ActionTap: ␣ -> m.
  CheckAction(tap_action_, ActionType::TAP, {ui::DomCode::SPACE}, {u"␣"},
              GetControlName(ActionType::TAP, u"␣"));
  CheckErrorState(GetButtonOptionsMenu(), tap_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);
  TapKeyboardKeyOnEditLabel(GetEditLabel(tap_action_list_item_, /*index=*/0),
                            ui::VKEY_M);
  CheckAction(tap_action_, ActionType::TAP, {ui::DomCode::US_M}, {u"m"},
              GetControlName(ActionType::TAP, u"m"));
  CheckErrorState(GetButtonOptionsMenu(), tap_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  // Modify the label for ActionMove and nothing is conflicted.
  // ActionMove: wasd -> lasd.
  CheckAction(move_action_, ActionType::MOVE,
              {ui::DomCode::US_W, ui::DomCode::US_A, ui::DomCode::US_S,
               ui::DomCode::US_D},
              {u"w", u"a", u"s", u"d"},
              GetControlName(ActionType::MOVE, u"wasd"));
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  TapKeyboardKeyOnEditLabel(GetEditLabel(move_action_list_item_, /*index=*/0),
                            ui::VKEY_L);
  CheckAction(move_action_, ActionType::MOVE,
              {ui::DomCode::US_L, ui::DomCode::US_A, ui::DomCode::US_S,
               ui::DomCode::US_D},
              {u"l", u"a", u"s", u"d"},
              GetControlName(ActionType::MOVE, u"lasd"));
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  // Modify the label for ActionMove and it is conflicted inside.
  // ActionMove: lasd -> ?ald.
  TapKeyboardKeyOnEditLabel(GetEditLabel(move_action_list_item_, /*index=*/2),
                            ui::VKEY_L);
  CheckAction(move_action_, ActionType::MOVE,
              {ui::DomCode::NONE, ui::DomCode::US_A, ui::DomCode::US_L,
               ui::DomCode::US_D},
              {u"?", u"a", u"l", u"d"},
              GetControlName(ActionType::MOVE, u"ald"));
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/true,
                  /*list_item_has_error=*/true);
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  // Modify the label for ActionMove and it is conflicted outside.
  // ActionTap: m -> ?
  // ActionMove: ?ald -> mald.
  TapKeyboardKeyOnEditLabel(GetEditLabel(move_action_list_item_, /*index=*/0),
                            ui::VKEY_M);
  CheckAction(tap_action_, ActionType::TAP, {ui::DomCode::NONE}, {u"?"},
              GetControlName(ActionType::TAP, u""));
  CheckErrorState(GetButtonOptionsMenu(), tap_action_list_item_,
                  /*menu_has_error=*/true,
                  /*list_item_has_error=*/true);
  CheckAction(move_action_, ActionType::MOVE,
              {ui::DomCode::US_M, ui::DomCode::US_A, ui::DomCode::US_L,
               ui::DomCode::US_D},
              {u"m", u"a", u"l", u"d"},
              GetControlName(ActionType::MOVE, u"mald"));
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  // Modify the label for ActionTap and it is conflicted outside.
  // ActionTap: ? -> d.
  // ActionMove: mald -> mal?.
  TapKeyboardKeyOnEditLabel(GetEditLabel(tap_action_list_item_, /*index=*/0),
                            ui::VKEY_D);
  CheckAction(tap_action_, ActionType::TAP, {ui::DomCode::US_D}, {u"d"},
              GetControlName(ActionType::TAP, u"d"));
  CheckErrorState(GetButtonOptionsMenu(), tap_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);
  CheckAction(move_action_, ActionType::MOVE,
              {ui::DomCode::US_M, ui::DomCode::US_A, ui::DomCode::US_L,
               ui::DomCode::NONE},
              {u"m", u"a", u"l", u"?"},
              GetControlName(ActionType::MOVE, u"mal"));
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/true,
                  /*list_item_has_error=*/true);
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());
}

TEST_F(EditLabelTest, TestEditingListLabelReservedKey) {
  // Press a reserved key on Action tap with no error state and then it shows
  // error state.
  ShowButtonOptionsMenu(tap_action_);
  FocusOnLabel(GetEditLabel(tap_action_list_item_, /*index=*/0));
  TapKeyboardKeyOnEditLabel(GetEditLabel(tap_action_list_item_, /*index=*/0),
                            ui::VKEY_ESCAPE);
  // Label is not changed.
  CheckAction(tap_action_, ActionType::TAP, {ui::DomCode::SPACE}, {u"␣"},
              GetControlName(ActionType::TAP, u"␣"));
  // Error state shows temporarily on list item view.
  CheckErrorState(GetButtonOptionsMenu(), tap_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/true);
  // Error state shows up temporarily and disappears after leaving focus.
  BlurOnLabel(GetEditLabel(tap_action_list_item_, /*index=*/0));
  CheckErrorState(GetButtonOptionsMenu(), tap_action_list_item_,
                  /*menu_has_error=*/false,
                  /*list_item_has_error=*/false);

  // Press a reserved key on Action move which is already in error state.
  // ActionMove: wasd -> wal?.
  FocusOnLabel(GetEditLabel(tap_action_list_item_, /*index=*/0));
  TapKeyboardKeyOnEditLabel(GetEditLabel(tap_action_list_item_, /*index=*/0),
                            ui::VKEY_D);
  CheckAction(move_action_, ActionType::MOVE,
              {ui::DomCode::US_W, ui::DomCode::US_A, ui::DomCode::US_S,
               ui::DomCode::NONE},
              {u"w", u"a", u"s", u"?"},
              GetControlName(ActionType::MOVE, u"was"));
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/true,
                  /*list_item_has_error=*/true);
  FocusOnLabel(GetEditLabel(move_action_list_item_, /*index=*/0));
  // Press a reserved key on Action move and error state still shows up.
  TapKeyboardKeyOnEditLabel(GetEditLabel(move_action_list_item_,
                                         /*index=*/0),
                            ui::VKEY_ESCAPE);
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/true,
                  /*list_item_has_error=*/true);
  BlurOnLabel(GetEditLabel(move_action_list_item_, /*index=*/0));
  // Error state still shows up after leaving focus.
  CheckErrorState(GetButtonOptionsMenu(), move_action_list_item_,
                  /*menu_has_error=*/true,
                  /*list_item_has_error=*/true);
}

TEST_F(EditLabelTest, TestEditingNewAction) {
  auto bounds = touch_injector_->content_bounds();
  controller_->AddNewAction(
      ActionType::MOVE, gfx::Point(bounds.width() / 2, bounds.height() / 2));
  auto* menu = GetButtonOptionsMenu();
  EXPECT_TRUE(menu);
  auto* action = GetAction(menu);
  EXPECT_TRUE(action->is_new());
  CheckAction(action, ActionType::MOVE,
              {ui::DomCode::NONE, ui::DomCode::NONE, ui::DomCode::NONE,
               ui::DomCode::NONE},
              {u"", u"", u"", u""}, GetControlName(ActionType::MOVE, u""));

  auto* label0 = GetEditLabel(menu, /*index=*/0);
  FocusOnLabel(label0);
  TapKeyboardKeyOnEditLabel(label0, ui::VKEY_A);
  EXPECT_TRUE(action->is_new());
  CheckAction(action, ActionType::MOVE,
              {ui::DomCode::US_A, ui::DomCode::NONE, ui::DomCode::NONE,
               ui::DomCode::NONE},
              {u"a", u"", u"", u""}, GetControlName(ActionType::MOVE, u"a"));

  auto* label1 = GetEditLabel(menu, /*index=*/1);
  FocusOnLabel(label1);
  TapKeyboardKeyOnEditLabel(label1, ui::VKEY_A);
  EXPECT_TRUE(action->is_new());
  CheckAction(action, ActionType::MOVE,
              {ui::DomCode::NONE, ui::DomCode::US_A, ui::DomCode::NONE,
               ui::DomCode::NONE},
              {u"", u"a", u"", u""}, GetControlName(ActionType::MOVE, u"a"));
}

TEST_F(EditLabelTest, TestHistograms) {
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(310, 10, 300, 500));
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Check histograms for editing list.
  const std::string editing_list_histogram_name =
      BuildGameControlsHistogramName(kEditingListFunctionTriggeredHistogram);
  std::map<EditingListFunction, int> expected_editing_list_histogram_values;
  LeftClickOn(GetEditLabel(tap_action_list_item_, /*index=*/0));
  MapIncreaseValueByOne(expected_editing_list_histogram_values,
                        EditingListFunction::kEditLabelFocused);
  VerifyHistogramValues(histograms, editing_list_histogram_name,
                        expected_editing_list_histogram_values);
  // There is a hover event recorded before this.
  VerifyEditingListFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/2u,
      static_cast<int64_t>(EditingListFunction::kEditLabelFocused));

  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_M, ui::EF_NONE);
  MapIncreaseValueByOne(expected_editing_list_histogram_values,
                        EditingListFunction::kKeyAssigned);
  VerifyHistogramValues(histograms, editing_list_histogram_name,
                        expected_editing_list_histogram_values);
  VerifyEditingListFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/3u,
      static_cast<int64_t>(EditingListFunction::kKeyAssigned));

  // Check histograms for button options menu.
  const std::string button_options_histogram_name =
      BuildGameControlsHistogramName(
          kButtonOptionsMenuFunctionTriggeredHistogram);
  std::map<ButtonOptionsMenuFunction, int>
      expected_button_options_histogram_values;
  auto* menu = ShowButtonOptionsMenu(move_action_);
  LeftClickOn(GetEditLabel(menu, /*index=*/1));
  MapIncreaseValueByOne(expected_button_options_histogram_values,
                        ButtonOptionsMenuFunction::kEditLabelFocused);
  VerifyHistogramValues(histograms, button_options_histogram_name,
                        expected_button_options_histogram_values);
  VerifyButtonOptionsMenuFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/1u, /*index=*/0u,
      static_cast<int64_t>(ButtonOptionsMenuFunction::kEditLabelFocused));

  event_generator->PressAndReleaseKey(ui::VKEY_N, ui::EF_NONE);
  // After assign a key, the focus is automatically moved to the next one.
  MapIncreaseValueByOne(expected_button_options_histogram_values,
                        ButtonOptionsMenuFunction::kEditLabelFocused);
  MapIncreaseValueByOne(expected_button_options_histogram_values,
                        ButtonOptionsMenuFunction::kKeyAssigned);
  VerifyHistogramValues(histograms, button_options_histogram_name,
                        expected_button_options_histogram_values);
  VerifyButtonOptionsMenuFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/3u, /*index=*/1u,
      static_cast<int64_t>(ButtonOptionsMenuFunction::kKeyAssigned));
  VerifyButtonOptionsMenuFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/3u, /*index=*/2u,
      static_cast<int64_t>(ButtonOptionsMenuFunction::kEditLabelFocused));
}

}  // namespace arc::input_overlay
