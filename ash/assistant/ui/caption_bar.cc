// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/caption_bar.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/views_util.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Appearance.
constexpr int kCaptionButtonSizeDip = 32;
constexpr int kPreferredHeightDip = 32;
constexpr int kVectorIconSizeDip = 12;

// CaptionButton ---------------------------------------------------------------

views::ImageButton* CreateCaptionButton(const gfx::VectorIcon& icon,
                                        int accessible_name_id,
                                        views::ButtonListener* listener) {
  return assistant::util::CreateImageButton(
      listener, icon, kCaptionButtonSizeDip, kVectorIconSizeDip,
      accessible_name_id, gfx::kGoogleGrey700);
}

}  // namespace

// CaptionBar ------------------------------------------------------------------

CaptionBar::CaptionBar() {
  InitLayout();
}

CaptionBar::~CaptionBar() = default;

const char* CaptionBar::GetClassName() const {
  return "CaptionBar";
}

bool CaptionBar::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_BROWSER_BACK:
      HandleButton(CaptionButtonId::kBack);
      break;
    default:
      NOTREACHED();
      return false;
  }

  // Don't let DialogClientView handle the accelerator.
  return true;
}

gfx::Size CaptionBar::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int CaptionBar::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void CaptionBar::ButtonPressed(views::Button* sender, const ui::Event& event) {
  auto id = static_cast<CaptionButtonId>(sender->id());
  HandleButton(id);
}

void CaptionBar::SetButtonVisible(CaptionButtonId id, bool visible) {
  views::View* button = GetViewByID(static_cast<int>(id));
  if (button)
    button->SetVisible(visible);
}

void CaptionBar::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kSpacingDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Back.
  auto* back_button =
      CreateCaptionButton(kWindowControlBackIcon, IDS_APP_LIST_BACK, this);
  back_button->set_id(static_cast<int>(CaptionButtonId::kBack));
  AddChildView(back_button);

  // Spacer.
  views::View* spacer = new views::View();
  AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Minimize.
  auto* minimize_button = CreateCaptionButton(kWindowControlMinimizeIcon,
                                              IDS_APP_ACCNAME_MINIMIZE, this);
  minimize_button->set_id(static_cast<int>(CaptionButtonId::kMinimize));
  AddChildView(minimize_button);

  // Close.
  auto* close_button =
      CreateCaptionButton(kWindowControlCloseIcon, IDS_APP_ACCNAME_CLOSE, this);
  close_button->set_id(static_cast<int>(CaptionButtonId::kClose));
  AddChildView(close_button);

  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));
}

void CaptionBar::HandleButton(CaptionButtonId id) {
  if (!GetViewByID(static_cast<int>(id))->visible())
    return;

  // If the delegate returns |true| it has handled the event and wishes to
  // prevent default behavior from being performed.
  if (delegate_ && delegate_->OnCaptionButtonPressed(id))
    return;

  switch (id) {
    case CaptionButtonId::kClose:
      GetWidget()->Close();
      break;
    case CaptionButtonId::kBack:
    case CaptionButtonId::kMinimize:
      // No default behavior defined.
      NOTIMPLEMENTED();
      break;
  }
}

}  // namespace ash
