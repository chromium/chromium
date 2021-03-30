// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/candidate_view.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/candidate_window_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {

namespace {

// VerticalCandidateLabel is used for rendering candidate text in
// the vertical candidate window.
class VerticalCandidateLabel : public views::Label {
 public:
  METADATA_HEADER(VerticalCandidateLabel);
  VerticalCandidateLabel() = default;
  VerticalCandidateLabel(const VerticalCandidateLabel&) = delete;
  VerticalCandidateLabel& operator=(const VerticalCandidateLabel&) = delete;
  ~VerticalCandidateLabel() override = default;

 private:
  // views::Label:
  // Returns the preferred size, but guarantees that the width has at
  // least kMinCandidateLabelWidth pixels.
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = Label::CalculatePreferredSize();
    size.SetToMax(gfx::Size(kMinCandidateLabelWidth, 0));
    size.SetToMin(gfx::Size(kMaxCandidateLabelWidth, size.height()));
    return size;
  }
};

BEGIN_METADATA(VerticalCandidateLabel, views::Label)
END_METADATA

// Creates the shortcut label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateShortcutLabel(
    ui::CandidateWindow::Orientation orientation,
    const ui::NativeTheme& theme) {
  auto shortcut_label = std::make_unique<views::Label>();

  // TODO(tapted): Get this FontList from views::style.
  if (orientation == ui::CandidateWindow::VERTICAL) {
    shortcut_label->SetFontList(shortcut_label->font_list().Derive(
        kFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  } else {
    shortcut_label->SetFontList(
        shortcut_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  }
  // TODO(satorux): Maybe we need to use language specific fonts for
  // candidate_label, like Chinese font for Chinese input method?

  // Setup paddings.
  const gfx::Insets kVerticalShortcutLabelInsets(1, 6, 1, 6);
  const gfx::Insets kHorizontalShortcutLabelInsets(1, 3, 1, 0);
  const gfx::Insets insets = (orientation == ui::CandidateWindow::VERTICAL
                                  ? kVerticalShortcutLabelInsets
                                  : kHorizontalShortcutLabelInsets);
  shortcut_label->SetBorder(views::CreateEmptyBorder(
      insets.top(), insets.left(), insets.bottom(), insets.right()));

  // Add decoration based on the orientation.
  if (orientation == ui::CandidateWindow::VERTICAL) {
    // Set the background color.
    SkColor blackish = color_utils::AlphaBlend(
        SK_ColorBLACK,
        theme.GetSystemColor(ui::NativeTheme::kColorId_WindowBackground),
        0.25f);
    shortcut_label->SetBackground(
        views::CreateSolidBackground(SkColorSetA(blackish, 0xE0)));
  }
  shortcut_label->SetElideBehavior(gfx::NO_ELIDE);

  return shortcut_label;
}

// Creates the candidate label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateCandidateLabel(
    ui::CandidateWindow::Orientation orientation) {
  std::unique_ptr<views::Label> candidate_label;

  if (orientation == ui::CandidateWindow::VERTICAL) {
    candidate_label = std::make_unique<VerticalCandidateLabel>();
  } else {
    candidate_label = std::make_unique<views::Label>();
  }

  // Change the font size.
  candidate_label->SetFontList(
      candidate_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  candidate_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  candidate_label->SetElideBehavior(gfx::NO_ELIDE);

  return candidate_label;
}

// Creates the annotation label, and return it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateAnnotationLabel(
    ui::CandidateWindow::Orientation orientation,
    const ui::NativeTheme& theme) {
  auto annotation_label = std::make_unique<views::Label>();

  // Change the font size and color.
  annotation_label->SetFontList(
      annotation_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  annotation_label->SetEnabledColor(
      theme.GetSystemColor(ui::NativeTheme::kColorId_LabelSecondaryColor));
  annotation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  annotation_label->SetElideBehavior(gfx::NO_ELIDE);

  return annotation_label;
}

}  // namespace

CandidateView::CandidateView(PressedCallback callback,
                             ui::CandidateWindow::Orientation orientation)
    : views::Button(std::move(callback)), orientation_(orientation) {
  SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));

  const ui::NativeTheme& theme = *GetNativeTheme();
  shortcut_label_ = AddChildView(CreateShortcutLabel(orientation, theme));
  candidate_label_ = AddChildView(CreateCandidateLabel(orientation));
  annotation_label_ = AddChildView(CreateAnnotationLabel(orientation, theme));

  if (orientation == ui::CandidateWindow::VERTICAL) {
    auto infolist_icon = std::make_unique<views::View>();
    infolist_icon->SetBackground(views::CreateSolidBackground(
        theme.GetSystemColor(ui::NativeTheme::kColorId_FocusedBorderColor)));
    infolist_icon_ = AddChildView(std::move(infolist_icon));
  }

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
}

void CandidateView::GetPreferredWidths(int* shortcut_width,
                                       int* candidate_width) {
  *shortcut_width = shortcut_label_->GetPreferredSize().width();
  *candidate_width = candidate_label_->GetPreferredSize().width();
}

void CandidateView::SetWidths(int shortcut_width, int candidate_width) {
  shortcut_width_ = shortcut_width;
  shortcut_label_->SetVisible(shortcut_width_ != 0);
  candidate_width_ = candidate_width;
}

void CandidateView::SetEntry(const ui::CandidateWindow::Entry& entry) {
  std::u16string label = entry.label;
  if (!label.empty() && orientation_ != ui::CandidateWindow::VERTICAL)
    label += u".";
  shortcut_label_->SetText(label);
  candidate_label_->SetText(entry.value);
  annotation_label_->SetText(entry.annotation);
}

void CandidateView::SetInfolistIcon(bool enable) {
  if (infolist_icon_)
    infolist_icon_->SetVisible(enable);
  SchedulePaint();
}

void CandidateView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  if (highlighted) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, false);
    ui::NativeTheme* theme = GetNativeTheme();
    SetBackground(views::CreateSolidBackground(theme->GetSystemColor(
        ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused)));
    SetBorder(views::CreateSolidBorder(
        1,
        theme->GetSystemColor(ui::NativeTheme::kColorId_FocusedBorderColor)));

    // Cancel currently focused one.
    for (View* view : parent()->children()) {
      if (view != this)
        static_cast<CandidateView*>(view)->SetHighlighted(false);
    }
  } else {
    SetBackground(nullptr);
    SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));
  }
  SchedulePaint();
}

void CandidateView::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  int text_style = GetState() == STATE_DISABLED ? views::style::STYLE_DISABLED
                                                : views::style::STYLE_PRIMARY;
  shortcut_label_->SetEnabledColor(views::style::GetColor(
      *shortcut_label_, views::style::CONTEXT_LABEL, text_style));
  if (GetState() == STATE_PRESSED)
    SetHighlighted(true);
}

bool CandidateView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location())) {
    // Moves the drag target to the sibling view.
    gfx::Point location_in_widget(event.location());
    ConvertPointToWidget(this, &location_in_widget);
    for (View* view : parent()->children()) {
      if (view == this)
        continue;
      gfx::Point location_in_sibling(location_in_widget);
      ConvertPointFromWidget(view, &location_in_sibling);
      if (view->HitTestPoint(location_in_sibling)) {
        GetWidget()->GetRootView()->SetMouseAndGestureHandler(view);
        auto* sibling = static_cast<CandidateView*>(view);
        sibling->SetHighlighted(true);
        return view->OnMouseDragged(ui::MouseEvent(event, this, sibling));
      }
    }

    return false;
  }

  return views::Button::OnMouseDragged(event);
}

void CandidateView::Layout() {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  int x = 0;
  shortcut_label_->SetBounds(x, 0, shortcut_width_, height());
  if (shortcut_width_ > 0)
    x += shortcut_width_ + padding_width;
  candidate_label_->SetBounds(x, 0, candidate_width_, height());
  x += candidate_width_ + padding_width;

  int right = bounds().right();
  if (infolist_icon_ && infolist_icon_->GetVisible()) {
    infolist_icon_->SetBounds(
        right - kInfolistIndicatorIconWidth - kInfolistIndicatorIconPadding,
        kInfolistIndicatorIconPadding, kInfolistIndicatorIconWidth,
        height() - kInfolistIndicatorIconPadding * 2);
    right -= kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2;
  }
  annotation_label_->SetBounds(x, 0, right - x, height());
}

gfx::Size CandidateView::CalculatePreferredSize() const {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  gfx::Size size;
  if (shortcut_label_->GetVisible()) {
    size = shortcut_label_->GetPreferredSize();
    size.SetToMax(gfx::Size(shortcut_width_, 0));
    size.Enlarge(padding_width, 0);
  }
  gfx::Size candidate_size = candidate_label_->GetPreferredSize();
  candidate_size.SetToMax(gfx::Size(candidate_width_, 0));
  size.Enlarge(candidate_size.width() + padding_width, 0);
  size.SetToMax(candidate_size);
  if (annotation_label_->GetVisible()) {
    gfx::Size annotation_size = annotation_label_->GetPreferredSize();
    size.Enlarge(annotation_size.width() + padding_width, 0);
    size.SetToMax(annotation_size);
  }

  // Reserves the margin for infolist_icon even if it's not visible.
  size.Enlarge(kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2,
               0);
  return size;
}

void CandidateView::SetPositionData(int index, int total) {
  candidate_index_ = index;
  total_candidates_ = total;
}

void CandidateView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(candidate_label_->GetText());
  node_data->role = ax::mojom::Role::kImeCandidate;
  // PosInSet needs to be incremented since |candidate_index_| is 0-based.
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             candidate_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             total_candidates_);
}

BEGIN_METADATA(CandidateView, views::Button)
END_METADATA

}  // namespace ime
}  // namespace ui
