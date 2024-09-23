// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_actions_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kActionButtonBetweenSpacing = 8;

}  // namespace

// SearchResultActionButton renders the button defined by SearchResult::Action.
class SearchResultActionButton : public IconButton {
  METADATA_HEADER(SearchResultActionButton, IconButton)

 public:
  SearchResultActionButton(SearchResultActionsView* parent,
                           const SearchResult::Action& action,
                           PressedCallback callback,
                           Type type,
                           const gfx::VectorIcon* icon,
                           const std::u16string& accessible_name);

  SearchResultActionButton(const SearchResultActionButton&) = delete;
  SearchResultActionButton& operator=(const SearchResultActionButton&) = delete;

  ~SearchResultActionButton() override {}

  // IconButton:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Updates the button visibility upon state change of the button or the
  // search result view associated with it.
  void UpdateOnStateChanged();

 private:
  int GetButtonRadius() const;

  raw_ptr<SearchResultActionsView> parent_;
  bool to_be_activate_by_long_press_ = false;
};

SearchResultActionButton::SearchResultActionButton(
    SearchResultActionsView* parent,
    const SearchResult::Action& action,
    PressedCallback callback,
    Type type,
    const gfx::VectorIcon* icon,
    const std::u16string& accessible_name)
    : IconButton(std::move(callback),
                 type,
                 icon,
                 action.tooltip_text,
                 /*is_togglable=*/false,
                 /*has_border=*/false),
      parent_(parent) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetVisible(false);

  StyleUtil::SetUpFocusRingForView(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate(
      base::BindRepeating([](const View* view) {
        const auto* v = views::AsViewClass<SearchResultActionButton>(view);
        CHECK(v);
        return v->HasFocus() || v->parent_->GetSelectedAction() == v->tag();
      }));
}

void SearchResultActionButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureLongPress:
      to_be_activate_by_long_press_ = true;
      event->SetHandled();
      break;
    case ui::EventType::kGestureEnd:
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

void SearchResultActionButton::UpdateOnStateChanged() {
  // Show button if the associated result row is hovered or selected, or one
  // of the action buttons is selected.
  SetVisible(parent_->IsSearchResultHoveredOrSelected());
  views::FocusRing::Get(this)->SchedulePaint();
}

int SearchResultActionButton::GetButtonRadius() const {
  return width() / 2;
}

BEGIN_METADATA(SearchResultActionButton)
END_METADATA

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

void SearchResultActionsView::HideActions() {
  for (views::View* child : children())
    child->SetVisible(false);
}

void SearchResultActionsView::UpdateButtonsOnStateChanged() {
  for (views::View* child : children())
    static_cast<SearchResultActionButton*>(child)->UpdateOnStateChanged();
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
  const gfx::VectorIcon* icon = nullptr;
  switch (action.type) {
    case SearchResultActionType::kRemove:
      icon = &ash::kSearchResultRemoveIcon;
      break;
  }

  DCHECK(icon);

  auto* const button = AddChildView(std::make_unique<SearchResultActionButton>(
      this, action,
      base::BindRepeating(
          &SearchResultActionsViewDelegate::OnSearchResultActionActivated,
          base::Unretained(delegate_), action_index),
      IconButton::Type::kMediumFloating, icon, action.tooltip_text));
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

BEGIN_METADATA(SearchResultActionsView)
END_METADATA

}  // namespace ash
