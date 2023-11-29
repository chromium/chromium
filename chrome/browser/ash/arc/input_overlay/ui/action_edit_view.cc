// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/animation/ink_drop.h"
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

}  // namespace

ActionEditView::ActionEditView(DisplayOverlayController* controller,
                               Action* action,
                               bool for_editing_list)
    : views::Button(base::BindRepeating(&ActionEditView::OnClicked,
                                        base::Unretained(this))),
      controller_(controller),
      action_(action) {
  // TODO(b/279117180): Replace with proper accessible name.
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA));
  SetUseDefaultFillLayout(true);
  SetNotifyEnterExitOnChild(true);
  auto* container = AddChildView(std::make_unique<views::TableLayoutView>());
  container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(14, kHorizontalInsets)));
  container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase,
      /*top_radius=*/kCornerRadius,
      /*bottom_radius=*/for_editing_list ? kCornerRadius : 0.0f,
      /*for_border_thickness=*/0));
  const int padding_width = for_editing_list
                                ? kNameTagAndLabelsPaddingForEditingList
                                : kNameTagAndLabelsPaddingForButtonOptionsMenu;
  container
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/views::LayoutAlignment::kStart,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/padding_width)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kStart,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  // TODO(b/274690042): Replace placeholder text with localized strings.
  name_tag_ = container->AddChildView(
      NameTag::CreateNameTag(u"Unassigned", for_editing_list));
  labels_view_ = container->AddChildView(EditLabels::CreateEditLabels(
      controller_, action_, name_tag_, /*should_update_title=*/true));

  name_tag_->SetMaximumWidth(
      (for_editing_list ? kEditingListWidth : kButtonOptionsMenuWidth) -
      2 * kEditingListInsideBorderInsets - 2 * kHorizontalInsets -
      padding_width - labels_view_->GetPreferredSize().width());

  // Set highlight path.
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), /*corner_radius=*/kCornerRadius));
}

ActionEditView::~ActionEditView() = default;

void ActionEditView::RemoveNewState() {
  labels_view_->RemoveNewState();
}

void ActionEditView::OnActionNameUpdated() {}

void ActionEditView::OnActionInputBindingUpdated() {
  labels_view_->OnActionInputBindingUpdated();
}

std::u16string ActionEditView::GetActionName() {
  return labels_view_->CalculateActionName();
}

void ActionEditView::OnClicked() {
  ClickCallback();
}

void ActionEditView::OnThemeChanged() {
  views::Button::OnThemeChanged();

  // Set up highlight and focus ring for `DeleteButton`.
  ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                        /*highlight_on_hover=*/true,
                                        /*highlight_on_focus=*/false);

  // `StyleUtil::SetUpInkDropForButton()` reinstalls the focus ring, so it
  // needs to set the focus ring size after calling
  // `StyleUtil::SetUpInkDropForButton()`.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetHaloThickness(kFocusRingHaloThickness);
}

BEGIN_METADATA(ActionEditView)
END_METADATA

}  // namespace arc::input_overlay
