// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_view.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

namespace {
constexpr int kSuggestionWindowCornerRadius = 5;

class SuggestionWindowBorder : public views::BubbleBorder {
 public:
  SuggestionWindowBorder()
      : views::BubbleBorder(views::BubbleBorder::NONE,
                            views::BubbleBorder::SMALL_SHADOW,
                            SK_ColorTRANSPARENT),
        offset_(0) {
    SetCornerRadius(kSuggestionWindowCornerRadius);
    set_use_theme_background_color(true);
  }
  ~SuggestionWindowBorder() override {}

  void set_offset(int offset) { offset_ = offset; }

 private:
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionWindowBorder);
};

}  // namespace

SuggestionWindowView::SuggestionWindowView(gfx::NativeView parent) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  suggestion_view_ = AddChildView(std::make_unique<SuggestionView>());
}

SuggestionWindowView::~SuggestionWindowView() = default;

views::Widget* SuggestionWindowView::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      std::make_unique<SuggestionWindowBorder>());
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void SuggestionWindowView::Hide() {
  GetWidget()->Close();
}

void SuggestionWindowView::Show(const base::string16& text,
                                const size_t confirmed_length,
                                const bool show_tab) {
  UpdateSuggestion(text, confirmed_length, show_tab);
  suggestion_view_->SetVisible(true);
  SizeToContents();
}

void SuggestionWindowView::UpdateSuggestion(const base::string16& text,
                                            const size_t confirmed_length,
                                            const bool show_tab) {
  suggestion_view_->SetView(text, confirmed_length, show_tab);

  std::unique_ptr<SuggestionWindowBorder> border =
      std::make_unique<SuggestionWindowBorder>();

  GetBubbleFrameView()->SetBubbleBorder(std::move(border));
  GetBubbleFrameView()->OnThemeChanged();
}

void SuggestionWindowView::SetBounds(const gfx::Rect& cursor_bounds) {
  SetAnchorRect(cursor_bounds);
}

const char* SuggestionWindowView::GetClassName() const {
  return "SuggestionWindowView";
}

}  // namespace ime
}  // namespace ui
