// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_finish_view.h"

#include "ash/style/style_util.h"
#include "base/bind.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace arc::input_overlay {

namespace {
// About the whole view.
constexpr int kViewHeight = 184;
constexpr int kSideMargin = 24;
// Space between children.
constexpr int kSpaceRow = 8;

// About button.
constexpr int kButtonHeight = 56;
constexpr int kButtonSideInset = 20;
constexpr int kButtonCornerRadius = 16;
constexpr SkColor kButtonTextColor = gfx::kGoogleGrey200;
constexpr SkColor kSaveButtonBackgroundColor = gfx::kGoogleBlue300;
constexpr SkColor kSaveButtonTextColor = gfx::kGoogleGrey900;
constexpr char kFontStyle[] = "Google Sans";
constexpr int kFontSize = 16;
// This color is same as the background color of input_mapping_view in kEdit
// mode and is used for buttons to decide what ink drop color should be. If the
// dark background color is set, then it will show the light ink drop color.
// Since we only need the dark mode for the kEdit mode, so dark background is
// passed for setting up the ink drop.
constexpr SkColor kEditBackgroundColor = SkColorSetA(SK_ColorBLACK, 0x99);

// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -6;
// Thickness of focus ring.
constexpr float kHaloThickness = 4;
}  // namespace

class EditFinishView::ChildButton : public views::LabelButton {
 public:
  explicit ChildButton(PressedCallback callback,
                       int text_source_id,
                       SkColor background_color,
                       SkColor text_color)
      : LabelButton(callback, l10n_util::GetStringUTF16(text_source_id)) {
    label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                       kFontSize, gfx::Font::Weight::MEDIUM));
    SetEnabledTextColors(text_color);
    SetAccessibleName(l10n_util::GetStringUTF16(text_source_id));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kButtonSideInset)));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetMinSize(gfx::Size(0, kButtonHeight));
    SetBackground(views::CreateRoundedRectBackground(background_color,
                                                     kButtonCornerRadius));

    // Set states.
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kButtonCornerRadius);
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHaloInset(kHaloInset);
    focus_ring->SetHaloThickness(kHaloThickness);
    focus_ring->SetColorId(ui::kColorAshEditFinishFocusRing);
    ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                          /*highlight_on_hover=*/true,
                                          /*highlight_on_focus=*/true,
                                          kEditBackgroundColor);
  }
  ~ChildButton() override = default;
};

// static
std::unique_ptr<EditFinishView> EditFinishView::BuildView(
    DisplayOverlayController* display_overlay_controller,
    const gfx::Size& parent_size) {
  auto menu_view_ptr =
      std::make_unique<EditFinishView>(display_overlay_controller);
  menu_view_ptr->Init(parent_size);

  return menu_view_ptr;
}

EditFinishView::EditFinishView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {}

EditFinishView::~EditFinishView() {}

void EditFinishView::Init(const gfx::Size& parent_size) {
  DCHECK(display_overlay_controller_);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpaceRow));
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));

  reset_button_ = AddChildView(std::make_unique<ChildButton>(
      base::BindRepeating(&EditFinishView::OnResetButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MODE_RESET, SK_ColorTRANSPARENT,
      kButtonTextColor));

  save_button_ = AddChildView(std::make_unique<ChildButton>(
      base::BindRepeating(&EditFinishView::OnSaveButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MODE_SAVE, kSaveButtonBackgroundColor,
      kSaveButtonTextColor));

  cancel_button_ = AddChildView(std::make_unique<ChildButton>(
      base::BindRepeating(&EditFinishView::OnCancelButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MODE_CANCEL, SK_ColorTRANSPARENT,
      kButtonTextColor));

  const int width = CalculateWidth();
  SetSize(gfx::Size(width, kViewHeight));
  SetPosition(
      gfx::Point(std::max(0, parent_size.width() - width - kSideMargin),
                 std::max(0, parent_size.height() / 3 - kViewHeight / 2)));
}

int EditFinishView::CalculateWidth() {
  int width = std::max(reset_button_->GetPreferredSize().width(),
                       save_button_->GetPreferredSize().width());
  width = std::max(width, cancel_button_->GetPreferredSize().width());
  return width;
}

void EditFinishView::OnResetButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->OnCustomizeRestore();
  // Take the focus from |ActionLabel|.
  reset_button_->RequestFocus();
}

void EditFinishView::OnSaveButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->OnCustomizeSave();
}

void EditFinishView::OnCancelButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->OnCustomizeCancel();
}

}  // namespace arc::input_overlay
