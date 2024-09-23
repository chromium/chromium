// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/page_switcher.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kNormalButtonRadius = 3;
constexpr int kSelectedButtonRadius = 4;
constexpr SkScalar kStrokeWidth = SkIntToScalar(2);

// Constants for the button strip that grows vertically.
// The padding on top/bottom side of each button.
constexpr int kVerticalButtonPadding = 0;

class PageSwitcherButton : public IconButton {
  METADATA_HEADER(PageSwitcherButton, IconButton)

 public:
  PageSwitcherButton(PressedCallback callback,
                     const std::u16string& accesible_name)
      : IconButton(std::move(callback),
                   IconButton::Type::kMediumFloating,
                   /*icon=*/nullptr,
                   accesible_name,
                   /*is_togglable=*/false,
                   /*has_border=*/false) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  PageSwitcherButton(const PageSwitcherButton&) = delete;
  PageSwitcherButton& operator=(const PageSwitcherButton&) = delete;

  ~PageSwitcherButton() override {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  // IconButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintButton(canvas, BuildPaintButtonInfo());
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
    info.color = GetColorProvider()->GetColor(kColorAshButtonIconColor);
    if (selected_) {
      info.style = cc::PaintFlags::kFill_Style;
      info.radius = SkIntToScalar(kSelectedButtonRadius);
      info.stroke_width = SkIntToScalar(0);
    } else {
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
};

// Gets PageSwitcherButton at |index| in |buttons|.
PageSwitcherButton* GetButtonByIndex(views::View* buttons, size_t index) {
  return static_cast<PageSwitcherButton*>(buttons->children()[index]);
}

BEGIN_METADATA(PageSwitcherButton)
END_METADATA

}  // namespace

PageSwitcher::PageSwitcher(PaginationModel* model)
    : model_(model), buttons_(new views::View) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  buttons_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kVerticalButtonPadding));

  AddChildView(buttons_.get());

  TotalPagesChanged(0, model->total_pages());
  SelectedPageChanged(-1, model->selected_page());
  model_->AddObserver(this);
}

PageSwitcher::~PageSwitcher() {
  if (model_)
    model_->RemoveObserver(this);
}

gfx::Size PageSwitcher::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::SizeBounds content_available_size(available_size);
  content_available_size.set_width(2 * PageSwitcher::kMaxButtonRadius);

  gfx::Insets insets = GetInsets();
  content_available_size.Enlarge(-insets.width(), -insets.height());

  gfx::Size buttons_size = buttons_->GetPreferredSize(content_available_size);

  // Always return a size with correct width so that container resize is not
  // needed when more pages are added.
  return gfx::Size(2 * PageSwitcher::kMaxButtonRadius,
                   buttons_size.height() + insets.height());
}

void PageSwitcher::Layout(PassKey) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  rect.ClampToCenteredSize(buttons_size);
  buttons_->SetBoundsRect(rect);
}

void PageSwitcher::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (!buttons_)
    return;
  for (views::View* child : buttons_->children()) {
    if (child->GetVisible())
      child->SchedulePaint();
  }
}

void PageSwitcher::HandlePageSwitch(const ui::Event& event) {
  if (!model_)
    return;

  const auto& children = buttons_->children();
  const auto it = base::ranges::find(children, event.target());
  DCHECK(it != children.end());
  const int page = std::distance(children.begin(), it);
  if (page == model_->selected_page())
    return;
  RecordPageSwitcherSource(event.IsGestureEvent() ? kTouchPageIndicator
                                                  : kClickPageIndicator);
  model_->SelectPage(page, true /* animate */);
}

void PageSwitcher::TotalPagesChanged(int previous_page_count,
                                     int new_page_count) {
  if (!model_)
    return;

  buttons_->RemoveAllChildViews();
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageSwitcherButton* button =
        buttons_->AddChildView(std::make_unique<PageSwitcherButton>(
            base::BindRepeating(&PageSwitcher::HandlePageSwitch,
                                base::Unretained(this)),
            l10n_util::GetStringFUTF16(
                IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(i + 1),
                base::FormatNumber(model_->total_pages()))));
    button->SetSelected(i == model_->selected_page() ? true : false);
  }
  buttons_->SetVisible(model_->total_pages() > 1);
  PreferredSizeChanged();
}

void PageSwitcher::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0 &&
      static_cast<size_t>(old_selected) < buttons_->children().size())
    GetButtonByIndex(buttons_, static_cast<size_t>(old_selected))
        ->SetSelected(false);
  if (new_selected >= 0 &&
      static_cast<size_t>(new_selected) < buttons_->children().size())
    GetButtonByIndex(buttons_, static_cast<size_t>(new_selected))
        ->SetSelected(true);
}

BEGIN_METADATA(PageSwitcher)
END_METADATA

}  // namespace ash
