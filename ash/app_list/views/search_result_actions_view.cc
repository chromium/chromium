// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_actions_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
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
constexpr int kProductivityLauncherImageButtonSizeDip = 32;
constexpr int kActionButtonBetweenSpacing = 8;

int GetButtonSize() {
  if (features::IsProductivityLauncherEnabled())
    return kProductivityLauncherImageButtonSizeDip;
  return kImageButtonSizeDip;
}

}  // namespace

// SearchResultImageButton renders the button defined by SearchResult::Action.
class SearchResultImageButton : public views::ImageButton {
 public:
  SearchResultImageButton(SearchResultActionsView* parent,
                          const SearchResult::Action& action);

  SearchResultImageButton(const SearchResultImageButton&) = delete;
  SearchResultImageButton& operator=(const SearchResultImageButton&) = delete;

  ~SearchResultImageButton() override {}

  // views::ImageButton:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Updates the button visibility upon state change of the button or the
  // search result view associated with it.
  void UpdateOnStateChanged();

 private:
  // views::ImageButton:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  void SetButtonImage(const gfx::ImageSkia& source);

  int GetButtonRadius() const;
  const char* GetClassName() const override;

  SearchResultActionsView* parent_;
  const bool visible_on_hover_;
  bool to_be_activate_by_long_press_ = false;
};

SearchResultImageButton::SearchResultImageButton(
    SearchResultActionsView* parent,
    const SearchResult::Action& action)
    : parent_(parent), visible_on_hover_(action.visible_on_hover) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // Avoid drawing default dashed focus and draw customized focus in
  // OnPaintBackground();
  SetFocusPainter(nullptr);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](SearchResultImageButton* host) {
        const AppListColorProvider* const color_provider =
            AppListColorProvider::Get();
        const SkColor bg_color = color_provider->GetSearchBoxBackgroundColor();
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()),
            features::IsProductivityLauncherEnabled()
                ? SK_ColorTRANSPARENT
                /*productivity launcher does not use inkdrop highlights*/
                : color_provider->GetInkDropBaseColor(bg_color));
        highlight->set_visible_opacity(
            features::IsProductivityLauncherEnabled()
                ? 0 /*productivity launcher does not use inkdrop highlights*/
                : color_provider->GetInkDropOpacity(bg_color));
        return highlight;
      },
      this));
  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](SearchResultImageButton* host)
          -> std::unique_ptr<views::InkDropRipple> {
        const gfx::Point center = host->GetLocalBounds().CenterPoint();
        const int ripple_radius = host->GetButtonRadius();
        gfx::Rect bounds(center.x() - ripple_radius, center.y() - ripple_radius,
                         2 * ripple_radius, 2 * ripple_radius);
        const AppListColorProvider* const color_provider =
            AppListColorProvider::Get();
        const SkColor bg_color = color_provider->GetSearchBoxBackgroundColor();
        return std::make_unique<views::FloodFillInkDropRipple>(
            host->size(), host->GetLocalBounds().InsetsFrom(bounds),
            views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            features::IsProductivityLauncherEnabled()
                ? color_provider->GetInvertedInkDropBaseColor(bg_color)
                : color_provider->GetInkDropBaseColor(bg_color),
            features::IsProductivityLauncherEnabled()
                ? color_provider->GetInvertedInkDropOpacity(bg_color)
                : color_provider->GetInkDropOpacity(bg_color));
      },
      this));

  SetPreferredSize({GetButtonSize(), GetButtonSize()});
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  SetButtonImage(action.image);

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

void SearchResultImageButton::UpdateOnStateChanged() {
  // Show button if the associated result row is hovered or selected, or one
  // of the action buttons is selected.
  if (visible_on_hover_)
    SetVisible(parent_->IsSearchResultHoveredOrSelected());
}

void SearchResultImageButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (HasFocus() || parent_->GetSelectedAction() == tag()) {
    PaintFocusRing(canvas, GetLocalBounds().CenterPoint(), GetButtonRadius());
  }
}

void SearchResultImageButton::SetButtonImage(const gfx::ImageSkia& source) {
  SetImage(views::ImageButton::STATE_NORMAL, source);
}

int SearchResultImageButton::GetButtonRadius() const {
  return width() / 2;
}

const char* SearchResultImageButton::GetClassName() const {
  return "SearchResultImageButton";
}

SearchResultActionsView::SearchResultActionsView(
    SearchResultActionsViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kActionButtonBetweenSpacing));
}

SearchResultActionsView::~SearchResultActionsView() {}

void SearchResultActionsView::SetActions(const SearchResult::Actions& actions) {
  if (selected_action_.has_value())
    selected_action_.reset();
  subscriptions_.clear();
  RemoveAllChildViews();

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

views::View* SearchResultActionsView::GetSelectedView() {
  DCHECK(HasSelectedAction());

  int selected_action = GetSelectedAction();
  for (views::View* child : children()) {
    if (static_cast<views::Button*>(child)->tag() == selected_action)
      return child;
  }

  return nullptr;
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
  button->SetCallback(base::BindRepeating(
      &SearchResultActionsViewDelegate::OnSearchResultActionActivated,
      base::Unretained(delegate_), action_index));
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

}  // namespace ash
