// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/page_switcher.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/pagination_model.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/skia_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

constexpr int kNormalButtonRadius = 4;
constexpr int kSelectedButtonRadius = 5;
constexpr int kInkDropRadius = 16;
constexpr int kMaxButtonRadius = 16;
constexpr int kPreferredButtonStripWidth = kMaxButtonRadius * 2;
constexpr SkScalar kStrokeWidth = SkIntToScalar(2);

// Constants for the button strip that grows vertically.
// The padding on top/bottom side of each button.
constexpr int kVerticalButtonPadding = 0;
// The selected button color.
constexpr SkColor kVerticalSelectedButtonColor =
    SkColorSetARGB(255, 232, 234, 237);
// The normal button color (54% white).
constexpr SkColor kVerticalNormalColor = SkColorSetARGB(255, 232, 234, 237);
constexpr SkColor kVerticalInkDropBaseColor = SkColorSetRGB(241, 243, 244);
constexpr SkColor kVerticalInkDropRippleColor =
    SkColorSetA(kVerticalInkDropBaseColor, 15);
constexpr SkColor kVerticalInkDropHighlightColor =
    SkColorSetA(kVerticalInkDropBaseColor, 20);

// Constants for the button strip that grows horizontally.
// The padding on left/right side of each button.
constexpr int kHorizontalButtonPadding = 6;
// The normal button color (54% black).
constexpr SkColor kHorizontalNormalColor = SkColorSetA(SK_ColorBLACK, 138);

class PageSwitcherButton : public views::Button {
 public:
  PageSwitcherButton(views::ButtonListener* listener, bool vertical)
      : views::Button(listener), vertical_(vertical) {
    if (vertical)
      SetInkDropMode(InkDropMode::ON);
  }

  ~PageSwitcherButton() override {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kMaxButtonRadius * 2, kMaxButtonRadius * 2);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintButton(canvas, BuildPaintButtonInfo());
  }

 protected:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        Button::CreateDefaultInkDropImpl();
    ink_drop->SetShowHighlightOnHover(true);
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    return std::move(ink_drop);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), kInkDropRadius);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = GetLocalBounds().CenterPoint();
    gfx::Rect bounds(center.x() - kMaxButtonRadius,
                     center.y() - kMaxButtonRadius, 2 * kMaxButtonRadius,
                     2 * kMaxButtonRadius);
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), GetLocalBounds().InsetsFrom(bounds),
        GetInkDropCenterBasedOnLastEvent(), kVerticalInkDropRippleColor, 1.0f);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(
            kVerticalInkDropHighlightColor, kInkDropRadius));
  }

  void NotifyClick(const ui::Event& event) override {
    Button::NotifyClick(event);
    GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
  }

 private:
  // Stores the information of how to paint the button.
  struct PaintButtonInfo {
    SkColor color;
    cc::PaintFlags::Style style;
    SkScalar radius;
    SkScalar stroke_width;
  };

  // Returns the information of how to paint selected/normal button.
  PaintButtonInfo BuildPaintButtonInfo() {
    PaintButtonInfo info;
    if (selected_) {
      info.color =
          vertical_ ? kVerticalSelectedButtonColor : kHorizontalNormalColor;
      info.style = cc::PaintFlags::kFill_Style;
      info.radius = SkIntToScalar(kSelectedButtonRadius);
      info.stroke_width = SkIntToScalar(0);
    } else {
      info.color = vertical_ ? kVerticalNormalColor : kHorizontalNormalColor;
      info.style = cc::PaintFlags::kStroke_Style;
      info.radius = SkIntToScalar(kNormalButtonRadius);
      info.stroke_width = kStrokeWidth;
    }
    return info;
  }

  // Paints a button based on the |info|.
  void PaintButton(gfx::Canvas* canvas, PaintButtonInfo info) {
    gfx::Rect rect(GetContentsBounds());
    SkPath path;
    path.addCircle(rect.CenterPoint().x(), rect.CenterPoint().y(), info.radius);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(info.style);
    flags.setColor(info.color);
    flags.setStrokeWidth(info.stroke_width);
    canvas->DrawPath(path, flags);
  }

  // If this button is selected, set to true. By default, set to false;
  bool selected_ = false;

  // True if the page switcher button strip should grow vertically.
  const bool vertical_;

  DISALLOW_COPY_AND_ASSIGN(PageSwitcherButton);
};

// Gets PageSwitcherButton at |index| in |buttons|.
PageSwitcherButton* GetButtonByIndex(views::View* buttons, int index) {
  return static_cast<PageSwitcherButton*>(buttons->child_at(index));
}

}  // namespace

PageSwitcher::PageSwitcher(PaginationModel* model, bool vertical)
    : model_(model), buttons_(new views::View), vertical_(vertical) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (vertical_) {
    buttons_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(), kVerticalButtonPadding));
  } else {
    buttons_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kHorizontal, gfx::Insets(),
        kHorizontalButtonPadding));
  }

  AddChildView(buttons_);

  TotalPagesChanged();
  SelectedPageChanged(-1, model->selected_page());
  model_->AddObserver(this);
}

PageSwitcher::~PageSwitcher() {
  if (model_)
    model_->RemoveObserver(this);
}

gfx::Size PageSwitcher::CalculatePreferredSize() const {
  // Always return a size with correct width so that container resize is not
  // needed when more pages are added.
  if (vertical_) {
    return gfx::Size(kPreferredButtonStripWidth,
                     buttons_->GetPreferredSize().height());
  }
  return gfx::Size(buttons_->GetPreferredSize().width(),
                   kPreferredButtonStripWidth);
}

void PageSwitcher::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  rect.ClampToCenteredSize(buttons_size);
  buttons_->SetBoundsRect(rect);
}

void PageSwitcher::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (!model_ || ignore_button_press_)
    return;

  for (int i = 0; i < buttons_->child_count(); ++i) {
    if (sender == static_cast<views::Button*>(buttons_->child_at(i))) {
      if (model_->selected_page() == i)
        break;
      UMA_HISTOGRAM_ENUMERATION(
          kAppListPageSwitcherSourceHistogram,
          event.IsGestureEvent() ? kTouchPageIndicator : kClickPageIndicator,
          kMaxAppListPageSwitcherSource);
      model_->SelectPage(i, true /* animate */);
      break;
    }
  }
}

void PageSwitcher::TotalPagesChanged() {
  if (!model_)
    return;

  buttons_->RemoveAllChildViews(true);
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageSwitcherButton* button = new PageSwitcherButton(this, vertical_);
    button->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(i + 1),
        base::FormatNumber(model_->total_pages())));
    button->SetSelected(i == model_->selected_page() ? true : false);
    buttons_->AddChildView(button);
  }
  buttons_->SetVisible(model_->total_pages() > 1);
  Layout();
}

void PageSwitcher::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0 && old_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, old_selected)->SetSelected(false);
  if (new_selected >= 0 && new_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, new_selected)->SetSelected(true);
}

void PageSwitcher::TransitionStarted() {}

void PageSwitcher::TransitionChanged() {}

void PageSwitcher::TransitionEnded() {}

}  // namespace app_list
