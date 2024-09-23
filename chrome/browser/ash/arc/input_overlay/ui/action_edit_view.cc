// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/rounded_container.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/table_layout_view.h"

namespace arc::input_overlay {

namespace {

constexpr float kCornerRadius = 16.0f;

constexpr int kHorizontalInsets = 16;

constexpr int kNameTagAndLabelsPaddingForButtonOptionsMenu = 20;
constexpr int kNameTagAndLabelsPaddingForEditingList = 12;

constexpr int kFocusRingHaloInset = -5;
constexpr int kFocusRingHaloThickness = 2;

void SetUpInkDrop(ActionEditView* host,
                  bool highlight_on_hover,
                  bool highlight_on_focus) {
  CHECK(host);
  auto* ink_drop = views::InkDrop::Get(host);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  host->SetHasInkDropActionOnClick(true);

  ink_drop->SetCreateInkDropCallback(base::BindRepeating(
      [](ActionEditView* host, bool highlight_on_hover,
         bool highlight_on_focus) {
        return views::InkDrop::CreateInkDropForFloodFillRipple(
            views::InkDrop::Get(host), highlight_on_hover, highlight_on_focus);
      },
      host, highlight_on_hover, highlight_on_focus));

  const auto* color_provider = host->GetColorProvider();
  CHECK(color_provider);

  const bool is_dark =
      ash::DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
  const SkColor ink_drop_color =
      is_dark ? SkColorSetA(
                    color_provider->GetColor(cros_tokens::kCrosRefPrimary50),
                    GetAlpha(/*percent=*/0.4f))
              : SkColorSetA(
                    color_provider->GetColor(cros_tokens::kCrosRefPrimary70),
                    GetAlpha(/*percent=*/0.44f));

  ink_drop->SetCreateRippleCallback(base::BindRepeating(
      [](ActionEditView* host,
         SkColor color) -> std::unique_ptr<views::InkDropRipple> {
        auto* ink_drop = views::InkDrop::Get(host);
        return std::make_unique<views::FloodFillInkDropRipple>(
            const_cast<views::InkDropHost*>(ink_drop), host->size(),
            gfx::Insets(), ink_drop->GetInkDropCenterBasedOnLastEvent(), color,
            1.0f);
      },
      host, ink_drop_color));

  ink_drop->SetCreateHighlightCallback(base::BindRepeating(
      [](ActionEditView* host, SkColor color) {
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()), color);
        // Opacity is already set the in `color`.
        highlight->set_visible_opacity(1.0f);
        return highlight;
      },
      host, ink_drop_color));
}

}  // namespace

ActionEditView::ActionEditView(DisplayOverlayController* controller,
                               Action* action,
                               bool for_editing_list)
    : views::Button(base::BindRepeating(&ActionEditView::OnClicked,
                                        base::Unretained(this))),
      controller_(controller),
      action_(action),
      for_editing_list_(for_editing_list) {
  SetUseDefaultFillLayout(true);
  SetNotifyEnterExitOnChild(true);
  auto* container = AddChildView(std::make_unique<views::TableLayoutView>());
  container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(14, kHorizontalInsets)));
  container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase,
      /*top_radius=*/for_editing_list ? kCornerRadius : 0.0f,
      /*bottom_radius=*/kCornerRadius));
  const int padding_width = for_editing_list
                                ? kNameTagAndLabelsPaddingForEditingList
                                : kNameTagAndLabelsPaddingForButtonOptionsMenu;
  container
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/
                  views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/padding_width)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  // TODO(b/274690042): Replace placeholder text with localized strings.
  name_tag_ = container->AddChildView(
      NameTag::CreateNameTag(u"Unassigned", for_editing_list));
  labels_view_ = container->AddChildView(EditLabels::CreateEditLabels(
      controller_, action_, name_tag_, for_editing_list));

  name_tag_->SetAvailableWidth(
      (for_editing_list ? kEditingListWidth : kButtonOptionsMenuWidth) -
      2 * kEditingListInsideBorderInsets - 2 * kHorizontalInsets -
      padding_width - labels_view_->GetPreferredSize().width());

  // Set a11y name after `labels_view_` is added.
  GetViewAccessibility().SetName(CalculateAccessibleLabel());

  // Set highlight path.
  views::HighlightPathGenerator::Install(
      this,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), for_editing_list
                             ? gfx::RoundedCornersF(kCornerRadius)
                             : gfx::RoundedCornersF(0.0f, 0.0f, kCornerRadius,
                                                    kCornerRadius)));

  // Set focus ring.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetHaloThickness(kFocusRingHaloThickness);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
}

ActionEditView::~ActionEditView() = default;

void ActionEditView::OnActionInputBindingUpdated() {
  labels_view_->OnActionInputBindingUpdated();
  if (for_editing_list_) {
    GetViewAccessibility().SetName(CalculateAccessibleLabel());
  }
}

void ActionEditView::RemoveNewState() {
  labels_view_->RemoveNewState();
}

std::u16string ActionEditView::CalculateActionName() {
  return labels_view_->CalculateActionName();
}

void ActionEditView::PerformPulseAnimation() {
  labels_view_->PerformPulseAnimationOnFirstLabel();
}

void ActionEditView::OnClicked() {
  ClickCallback();
}

std::u16string ActionEditView::CalculateAccessibleLabel() const {
  if (!for_editing_list_) {
    return l10n_util::GetStringUTF16(
        IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ACTION_EDIT_BUTTON_A11Y_LABEL);
  }

  const std::u16string keys_string =
      labels_view_->CalculateKeyListForA11yLabel();

  // "Selected key is w. Tap on the button to edit the control" or
  // "Selected keys are w, a, s, d. Tap on the button to edit the control".
  return l10n_util::GetStringFUTF16(
      keys_string.find(',') == std::string::npos
          ? IDS_INPUT_OVERLAY_EDITING_LIST_ITEM_BUTTON_CONTAINER_A11Y_TPL
          : IDS_INPUT_OVERLAY_EDITING_LIST_ITEM_JOYSTICK_CONTAINER_A11Y_TPL,
      keys_string);
}

void ActionEditView::OnThemeChanged() {
  views::Button::OnThemeChanged();

  SetUpInkDrop(this, /*highlight_on_hover=*/for_editing_list_,
               /*highlight_on_focus=*/for_editing_list_);
}

BEGIN_METADATA(ActionEditView)
END_METADATA

}  // namespace arc::input_overlay
