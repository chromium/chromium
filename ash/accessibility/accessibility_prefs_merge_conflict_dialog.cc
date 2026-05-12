// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_prefs_merge_conflict_dialog.h"

#include "ash/accessibility/accessibility_prefs_merge_conflict_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/check_is_test.h"
#include "base/memory/raw_ref.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kScrollViewCornerRadiusDip = 16;

// Inset the scroll bar to avoid the rounded corners at top and bottom.
constexpr auto kScrollBarInsetsDip =
    gfx::Insets::VH(kScrollViewCornerRadiusDip, 0);

constexpr int kScrollerContainerHeightDip = 300;

constexpr int kContentsRoundingDip = 20;

struct AccessibilityPrefUiConfig {
  const char* pref_name;
  raw_ref<const gfx::VectorIcon> icon;
  int label_id;
};

constexpr AccessibilityPrefUiConfig kAccessibilityPrefUiTable[] = {
    // Batch 1
    {prefs::kAccessibilityHighContrastEnabled,
     raw_ref<const gfx::VectorIcon>(kSystemMenuAccessibilityContrastIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE},
    {prefs::kAccessibilityLargeCursorEnabled,
     raw_ref<const gfx::VectorIcon>(kQuickSettingsA11yLargeMouseCursorIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR},
    {prefs::kAccessibilityCursorHighlightEnabled,
     raw_ref<const gfx::VectorIcon>(kQuickSettingsA11yHighlightMouseCursorIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_MOUSE_CURSOR},
    {prefs::kAccessibilityCaretHighlightEnabled,
     raw_ref<const gfx::VectorIcon>(kQuickSettingsA11yHighlightTextCaretIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_CARET_HIGHLIGHT},
    {prefs::kAccessibilityFocusHighlightEnabled,
     raw_ref<const gfx::VectorIcon>(
         kQuickSettingsA11yHighlightKeyboardFocusIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_KEYBOARD_FOCUS},

    // Batch 3
    {prefs::kDockedMagnifierEnabled,
     raw_ref<const gfx::VectorIcon>(
         kSystemMenuAccessibilityDockedMagnifierIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DOCKED_MAGNIFIER},
    {prefs::kAccessibilitySelectToSpeakEnabled,
     raw_ref<const gfx::VectorIcon>(kSystemMenuAccessibilitySelectToSpeakIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK},
    {prefs::kAccessibilityScreenMagnifierEnabled,
     raw_ref<const gfx::VectorIcon>(
         kSystemMenuAccessibilityFullscreenMagnifierIcon),
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER},
};

// Create a non-clickable and non-focusable toggle button at the end.
Switch* AddToggleToEnd(HoverHighlightView* item) {
  auto toggle = std::make_unique<Switch>();
  Switch* toggle_ptr = toggle.get();
  item->AddChildView(std::move(toggle));
  item->AddRightView(toggle_ptr);
  return toggle_ptr;
}

}  // namespace

// static
std::unique_ptr<views::Widget>
AccessibilityPrefsMergeConflictDialog::CreateAndShow(
    std::unique_ptr<AccessibilityPrefsMergeConflictController> controller,
    base::OnceCallback<void()> on_dismissed) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new AccessibilityPrefsMergeConflictDialog(
      std::move(controller), std::move(on_dismissed));

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));

  // `params` does not specify the initial bounds. Therefore, the dialog shows
  // at the center of the display.
  widget->Show();
  return widget;
}

AccessibilityPrefsMergeConflictDialog::AccessibilityPrefsMergeConflictDialog(
    std::unique_ptr<AccessibilityPrefsMergeConflictController> controller,
    base::OnceCallback<void()> on_dismissed)
    : controller_(std::move(controller)),
      on_dismissed_(std::move(on_dismissed)) {
  views::Builder<SystemDialogDelegateView>(this)
      .SetTitleText(l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_PREFS_CONFLICT_RESOLUTION_DIALOG_TITLE))
      .SetDescription(l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_PREFS_CONFLICT_RESOLUTION_DIALOG_DESCRIPTION))
      .SetAcceptButtonText(l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_PREFS_CONFLICT_RESOLUTION_DIALOG_ACCEPT_BUTTON_TEXT))
      .SetAcceptCallback(base::BindOnce(
          &AccessibilityPrefsMergeConflictDialog::OnResolvePrefsAccepted,
          weak_factory_.GetWeakPtr()))
      .SetCancelButtonText(l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_PREFS_CONFLICT_RESOLUTION_DIALOG_CANCEL_BUTTON_TEXT))
      .SetCancelCallback(base::BindOnce(
          &AccessibilityPrefsMergeConflictDialog::OnShowAccessibilitySettings,
          weak_factory_.GetWeakPtr()))
      .BuildChildren();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true);
  SetModalType(ui::mojom::ModalType::kSystem);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque, kContentsRoundingDip));
  SetButtonContainerAlignment(views::LayoutAlignment::kEnd);
  SetTopContentView(views::Builder<views::ImageView>()
                        .SetImage(ui::ImageModel::FromVectorIcon(
                            kUnifiedMenuAccessibilityIcon))
                        .Build());
  SetTopContentAlignment(views::LayoutAlignment::kStart);

  auto scroller = std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled);
  scroller->SetDrawOverflowIndicator(false);
  scroller->ClipHeightTo(0, kScrollerContainerHeightDip);

  auto* scroll_content = scroller->SetContents(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .Build());

  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kScrollBarInsetsDip);
  vertical_scroll->SetAlwaysShowThumb(true);
  scroller->SetVerticalScrollBar(std::move(vertical_scroll));
  scroller->SetPaintToLayer();
  scroller->layer()->SetFillsBoundsOpaquely(false);
  scroller->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kScrollViewCornerRadiusDip));

  // Override the default theme-based color to remove the background.
  scroller->SetBackgroundColor(std::nullopt);

  views::View* main_container =
      scroll_content->AddChildView(std::make_unique<RoundedContainer>());

  BuildResolutionList(main_container);

  SetMiddleContentView(std::move(scroller));

  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kUnbounded));
}

AccessibilityPrefsMergeConflictDialog::
    ~AccessibilityPrefsMergeConflictDialog() = default;

void AccessibilityPrefsMergeConflictDialog::BuildResolutionList(
    views::View* main_container) {
  for (const auto& conflict : controller_->conflicts()) {
    bool match = false;

    // Find the UI metadata for this pref in kAccessibilityPrefUiTable.
    // Every conflict is expected to have a matching entry.
    for (const auto& config : kAccessibilityPrefUiTable) {
      if (conflict.pref_name != config.pref_name) {
        continue;
      }

      match = true;
      AddScrollListToggleItem(main_container, *config.icon,
                              l10n_util::GetStringUTF16(config.label_id),
                              conflict.pref_name,
                              conflict.local_value.GetBool());
      break;
    }

    // Assert that every conflict had a matching config.
    CHECK(match) << "No UI config found for pref: " << conflict.pref_name;
  }
}

HoverHighlightView*
AccessibilityPrefsMergeConflictDialog::AddScrollListToggleItem(
    views::View* container,
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    std::string_view pref_name,
    bool checked,
    bool enterprise_managed) {
  HoverHighlightView* item = AddScrollListItem(container, icon, text);
  item->SetAccessibilityState(
      checked ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
              : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);

  // TODO(https://crbug.com/479890756): Test and ensure that the behavior is as
  // expected for enterprise-enabled devices.
  if (enterprise_managed) {
    // Show the enterprise "building" icon at the end.
    item->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_ACCESSIBILITY_FEATURE_MANAGED, text));
    ui::ImageModel enterprise_managed_icon = ui::ImageModel::FromVectorIcon(
        kSystemMenuBusinessIcon, kColorAshIconColorPrimary, kMenuIconSize);
    item->AddRightIcon(enterprise_managed_icon,
                       enterprise_managed_icon.Size().width());
  } else {
    Switch* toggle = AddToggleToEnd(item);
    toggle->SetIsOn(checked);
    toggle->SetCanProcessEventsWithinSubtree(false);
    toggle->SetFocusBehavior(views::View::FocusBehavior::NEVER);

    item->SetCallback(base::BindRepeating(
        &AccessibilityPrefsMergeConflictDialog::OnPrefRowPressed,
        weak_factory_.GetWeakPtr(), item, pref_name));

    // Ignore the toggle for accessibility.
    auto& view_accessibility = toggle->GetViewAccessibility();
    view_accessibility.SetIsLeaf(true);
    view_accessibility.SetIsIgnored(true);
  }

  return item;
}

HoverHighlightView* AccessibilityPrefsMergeConflictDialog::AddScrollListItem(
    views::View* container,
    const gfx::VectorIcon& icon,
    const std::u16string& text) {
  // TODO(https://crbug.com/479890756): Evaluate if inheriting from
  // ViewClickListener and override OnViewClicked() is a cleaner solution.
  HoverHighlightView* item = container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/nullptr));
  if (icon.is_empty()) {
    item->AddLabelRow(text);
  } else {
    item->AddIconAndLabel(
        ui::ImageModel::FromVectorIcon(icon, cros_tokens::kCrosSysOnSurface),
        text);
  }
  views::FocusRing::Install(item);
  views::InstallRoundRectHighlightPathGenerator(item, gfx::Insets(2),
                                                /*corner_radius=*/0);
  views::FocusRing::Get(item)->SetColorId(cros_tokens::kCrosSysFocusRing);
  // Unset the focus painter set by `HoverHighlightView`.
  item->SetFocusPainter(nullptr);

  return item;
}

void AccessibilityPrefsMergeConflictDialog::WindowClosing() {
  if (on_dismissed_) {
    std::move(on_dismissed_).Run();
  }
}

void AccessibilityPrefsMergeConflictDialog::OnResolvePrefsAccepted() {
  GetWidget()->Close();
}

void AccessibilityPrefsMergeConflictDialog::OnShowAccessibilitySettings() {
  Shell::Get()->system_tray_model()->client()->ShowAccessibilitySettings();
  GetWidget()->Close();
}

void AccessibilityPrefsMergeConflictDialog::OnPrefRowPressed(
    HoverHighlightView* item,
    std::string_view pref_name) {
  auto* toggle = views::AsViewClass<Switch>(item->right_view());
  const bool new_state = !toggle->GetIsOn();
  toggle->SetIsOn(new_state);

  item->SetAccessibilityState(
      new_state ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
                : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);

  controller_->UpdateConflict(pref_name, base::Value(new_state));
}

BEGIN_METADATA(AccessibilityPrefsMergeConflictDialog)
END_METADATA

}  // namespace ash
