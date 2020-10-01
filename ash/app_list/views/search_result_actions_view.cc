// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_actions_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/numerics/ranges.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Image buttons.
constexpr int kImageButtonSizeDip = 40;
constexpr int kActionButtonBetweenSpacing = 8;

}  // namespace

// SearchResultImageButton renders the button defined by SearchResult::Action.
class SearchResultImageButton : public views::ImageButton {
 public:
  SearchResultImageButton(SearchResultActionsView* parent,
                          const SearchResult::Action& action);
  ~SearchResultImageButton() override {}

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::InkDropHostView:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  // Updates the button visibility upon state change of the button or the
  // search result view associated with it.
  void UpdateOnStateChanged();

 private:
  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  void SetButtonImage(const gfx::ImageSkia& source, int icon_dimension);

  int GetInkDropRadius() const;
  const char* GetClassName() const override;

  SearchResultActionsView* parent_;
  const bool visible_on_hover_;
  bool to_be_activate_by_long_press_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchResultImageButton);
};

SearchResultImageButton::SearchResultImageButton(
    SearchResultActionsView* parent,
    const SearchResult::Action& action)
    : ImageButton(parent),
      parent_(parent),
      visible_on_hover_(action.visible_on_hover) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // Avoid drawing default dashed focus and draw customized focus in
  // OnPaintBackground();
  SetFocusPainter(nullptr);
  SetInkDropMode(InkDropMode::ON);

  SetPreferredSize({kImageButtonSizeDip, kImageButtonSizeDip});
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  SetButtonImage(action.image,
                 AppListConfig::instance().search_list_icon_dimension());

  SetAccessibleName(action.tooltip_text);

  SetTooltipText(action.tooltip_text);

  SetVisible(!visible_on_hover_);
  views::InstallCircleHighlightPathGenerator(this);
}

void SearchResultImageButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      to_be_activate_by_long_press_ = true;
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      if (to_be_activate_by_long_press_) {
        NotifyClick(*event);
        SetState(STATE_NORMAL);
        to_be_activate_by_long_press_ = false;
        event->SetHandled();
      }
      break;
    default:
      break;
  }
  if (!event->handled())
    Button::OnGestureEvent(event);
}

std::unique_ptr<views::InkDropRipple>
SearchResultImageButton::CreateInkDropRipple() const {
  const gfx::Point center = GetLocalBounds().CenterPoint();
  const int ripple_radius = GetInkDropRadius();
  gfx::Rect bounds(center.x() - ripple_radius, center.y() - ripple_radius,
                   2 * ripple_radius, 2 * ripple_radius);
  SkColor ripple_color =
      AppListColorProvider::Get()->GetSearchResultViewInkDropColor();
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), ripple_color, 1.0f);
}

std::unique_ptr<views::InkDropHighlight>
SearchResultImageButton::CreateInkDropHighlight() const {
  SkColor ripple_color =
      AppListColorProvider::Get()->GetSearchResultViewHighlightColor();
  auto highlight = std::make_unique<views::InkDropHighlight>(gfx::SizeF(size()),
                                                             ripple_color);
  highlight->set_visible_opacity(1.f);
  return highlight;
}

void SearchResultImageButton::UpdateOnStateChanged() {
  // Show button if the associated result row is hovered or selected, or one
  // of the action buttons is selected.
  if (visible_on_hover_)
    SetVisible(parent_->IsSearchResultHoveredOrSelected());
}

void SearchResultImageButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (HasFocus() || parent_->GetSelectedAction() == tag()) {
    cc::PaintFlags circle_flags;
    circle_flags.setAntiAlias(true);
    circle_flags.setColor(
        AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
    circle_flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(GetLocalBounds().CenterPoint(), GetInkDropRadius(),
                       circle_flags);
  }
}

void SearchResultImageButton::SetButtonImage(const gfx::ImageSkia& source,
                                             int icon_dimension) {
  SetImage(views::ImageButton::STATE_NORMAL,
           gfx::ImageSkiaOperations::CreateResizedImage(
               source, skia::ImageOperations::RESIZE_BEST,
               gfx::Size(icon_dimension, icon_dimension)));
}

int SearchResultImageButton::GetInkDropRadius() const {
  return width() / 2;
}

const char* SearchResultImageButton::GetClassName() const {
  return "SearchResultImageButton";
}

SearchResultActionsView::SearchResultActionsView(
    SearchResultActionsViewDelegate* delegate)
    : delegate_(delegate) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kActionButtonBetweenSpacing));
}

SearchResultActionsView::~SearchResultActionsView() {}

void SearchResultActionsView::SetActions(const SearchResult::Actions& actions) {
  if (selected_action_.has_value())
    selected_action_.reset();
  subscriptions_.clear();
  RemoveAllChildViews(true);

  for (size_t i = 0; i < actions.size(); ++i)
    CreateImageButton(actions[i], i);
  PreferredSizeChanged();
}

bool SearchResultActionsView::IsValidActionIndex(size_t action_index) const {
  return action_index < GetActionCount();
}

bool SearchResultActionsView::IsSearchResultHoveredOrSelected() const {
  return delegate_->IsSearchResultHoveredOrSelected();
}

void SearchResultActionsView::UpdateButtonsOnStateChanged() {
  for (views::View* child : children())
    static_cast<SearchResultImageButton*>(child)->UpdateOnStateChanged();
}

const char* SearchResultActionsView::GetClassName() const {
  return "SearchResultActionsView";
}

bool SearchResultActionsView::SelectInitialAction(bool reverse_tab_order) {
  if (GetActionCount() == 0)
    return false;

  if (reverse_tab_order) {
    selected_action_ = GetActionCount() - 1;
  } else {
    selected_action_.reset();
  }
  UpdateButtonsOnStateChanged();
  return selected_action_.has_value();
}

bool SearchResultActionsView::SelectNextAction(bool reverse_tab_order) {
  if (GetActionCount() == 0)
    return false;

  // For reverse tab order, consider moving to non-selected state.
  if (reverse_tab_order) {
    if (!selected_action_.has_value())
      return false;

    if (selected_action_.value() == 0) {
      ClearSelectedAction();
      return true;
    }
  }

  const int next_index =
      selected_action_.value_or(-1) + (reverse_tab_order ? -1 : 1);
  if (!IsValidActionIndex(next_index))
    return false;

  selected_action_ = next_index;
  UpdateButtonsOnStateChanged();
  return true;
}

void SearchResultActionsView::NotifyA11yResultSelected() {
  DCHECK(HasSelectedAction());

  int selected_action = GetSelectedAction();
  for (views::View* child : children()) {
    if (static_cast<views::Button*>(child)->tag() == selected_action) {
      child->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
      return;
    }
  }
}

void SearchResultActionsView::ClearSelectedAction() {
  selected_action_.reset();
  UpdateButtonsOnStateChanged();
}

int SearchResultActionsView::GetSelectedAction() const {
  return selected_action_.value_or(-1);
}

bool SearchResultActionsView::HasSelectedAction() const {
  return selected_action_.has_value();
}

void SearchResultActionsView::CreateImageButton(
    const SearchResult::Action& action,
    int action_index) {
  auto* const button =
      AddChildView(std::make_unique<SearchResultImageButton>(this, action));
  button->set_tag(action_index);
  subscriptions_.push_back(button->AddStateChangedCallback(
      base::BindRepeating(&SearchResultActionsView::UpdateButtonsOnStateChanged,
                          base::Unretained(this))));
}

size_t SearchResultActionsView::GetActionCount() const {
  return children().size();
}

void SearchResultActionsView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void SearchResultActionsView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (!delegate_)
    return;

  DCHECK_GE(sender->tag(), 0);
  DCHECK_LT(sender->tag(), static_cast<int>(GetActionCount()));
  delegate_->OnSearchResultActionActivated(sender->tag(), event.flags());
}

}  // namespace ash
