// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/page_indicator_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kUnifiedPageIndicatorButtonRadius = 3;
constexpr int kInkDropRadius = 3 * kUnifiedPageIndicatorButtonRadius;

}  // namespace

// Button internally used in PageIndicatorView. Each button
// stores a page number which it switches to if pressed.
class PageIndicatorView::PageIndicatorButton : public views::Button {
 public:
  PageIndicatorButton(UnifiedSystemTrayController* controller, int page)
      : views::Button(base::BindRepeating(
            &UnifiedSystemTrayController::HandlePageSwitchAction,
            base::Unretained(controller),
            page)) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    views::InstallFixedSizeCircleHighlightPathGenerator(this, kInkDropRadius);

    const gfx::Point center = GetLocalBounds().CenterPoint();
    const gfx::Rect bounds(center.x() - kInkDropRadius,
                           center.y() - kInkDropRadius, 2 * kInkDropRadius,
                           2 * kInkDropRadius);
    StyleUtil::SetUpInkDropForButton(this, GetLocalBounds().InsetsFrom(bounds),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/false);
  }

  PageIndicatorButton(const PageIndicatorButton&) = delete;
  PageIndicatorButton& operator=(const PageIndicatorButton&) = delete;

  ~PageIndicatorButton() override {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kInkDropRadius * 2, kInkDropRadius * 2);
  }

  // views::Button:
  const char* GetClassName() const override { return "PageIndicatorView"; }

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::Rect rect(GetContentsBounds());

    const SkColor selected_color =
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(selected_ ? selected_color
                             : ColorUtil::GetDisabledColor(selected_color));
    canvas->DrawCircle(rect.CenterPoint(), kUnifiedPageIndicatorButtonRadius,
                       flags);
  }

  // views::Button:
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity |
                  StyleUtil::kHighlightOpacity);
    SchedulePaint();
  }

  bool selected() const { return selected_; }

 protected:
  // views::Button:
  void NotifyClick(const ui::Event& event) override {
    Button::NotifyClick(event);
    views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
        views::InkDropState::ACTION_TRIGGERED);
  }

 private:
  bool selected_ = false;
};

PageIndicatorView::PageIndicatorView(UnifiedSystemTrayController* controller,
                                     bool initially_expanded)
    : controller_(controller),
      model_(controller->model()->pagination_model()),
      expanded_amount_(initially_expanded ? 1 : 0),
      buttons_container_(new views::View) {
  SetVisible(initially_expanded);

  buttons_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets()));
  buttons_container_->SetPaintToLayer();
  buttons_container_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(buttons_container_);

  TotalPagesChanged(0, model_->total_pages());

  DCHECK(model_);
  model_->AddObserver(this);
}

PageIndicatorView::~PageIndicatorView() {
  model_->RemoveObserver(this);
}

gfx::Size PageIndicatorView::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, kPageIndicatorViewMaxHeight);
}

void PageIndicatorView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  gfx::Size buttons_container_size(buttons_container_->GetPreferredSize());
  rect.ClampToCenteredSize(buttons_container_size);
  buttons_container_->SetBoundsRect(rect);
}

const char* PageIndicatorView::GetClassName() const {
  return "PageIndicatorView";
}

void PageIndicatorView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  SetVisible(expanded_amount > 0.0);
  expanded_amount_ = expanded_amount;
  // TODO(amehfooz): Confirm animation curve with UX.
  buttons_container_->layer()->SetOpacity(
      std::max(0., 6 * expanded_amount_ - 5.));
  if (CalculatePreferredSize() != size())
    InvalidateLayout();
}

int PageIndicatorView::GetExpandedHeight() {
  return buttons_container_->GetPreferredSize().height();
}

void PageIndicatorView::TotalPagesChanged(int previous_page_count,
                                          int new_page_count) {
  DCHECK(model_);

  buttons_container_->RemoveAllChildViews();
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageIndicatorButton* button = new PageIndicatorButton(controller_, i);
    button->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(i + 1),
        base::FormatNumber(model_->total_pages())));
    button->SetSelected(i == model_->selected_page());
    buttons_container_->AddChildView(button);
  }
  buttons_container_->SetVisible(model_->total_pages() > 1);
  Layout();
}

PageIndicatorView::PageIndicatorButton* PageIndicatorView::GetButtonByIndex(
    int index) {
  return static_cast<PageIndicatorButton*>(
      buttons_container_->children().at(index));
}

void PageIndicatorView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  size_t total_children = buttons_container_->children().size();

  if (old_selected >= 0 && static_cast<size_t>(old_selected) < total_children)
    GetButtonByIndex(old_selected)->SetSelected(false);
  if (new_selected >= 0 && static_cast<size_t>(new_selected) < total_children)
    GetButtonByIndex(new_selected)->SetSelected(true);
}

bool PageIndicatorView::IsPageSelectedForTesting(int index) {
  return GetButtonByIndex(index)->selected();
}

}  // namespace ash
