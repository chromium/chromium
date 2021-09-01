// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/public_account_menu_view.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

constexpr int kMenuItemWidthDp = 192;
constexpr int kMenuItemHeightDp = 28;
constexpr int kRegularMenuItemLeftPaddingDp = 2;
constexpr int kGroupMenuItemLeftPaddingDp = 10;
constexpr int kNonEmptyHeight = 1;

namespace {

class MenuItemView : public views::Button {
 public:
  MenuItemView(const PublicAccountMenuView::Item& item,
               const PublicAccountMenuView::OnHighlight& on_highlight)
      : views::Button(base::BindRepeating(&MenuItemView::ButtonPressed,
                                          base::Unretained(this))),
        item_(item),
        on_highlight_(on_highlight) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_suppress_default_focus_handling();
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    SetPreferredSize(gfx::Size(kMenuItemWidthDp, kMenuItemHeightDp));

    auto spacing = std::make_unique<NonAccessibleView>();
    spacing->SetPreferredSize(gfx::Size(item.is_group
                                            ? kRegularMenuItemLeftPaddingDp
                                            : kGroupMenuItemLeftPaddingDp,
                                        kNonEmptyHeight));
    AddChildView(std::move(spacing));

    auto label = std::make_unique<views::Label>(base::UTF8ToUTF16(item.title));
    label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(std::move(label));

    if (item.selected)
      SetBackground(views::CreateSolidBackground(SK_ColorGRAY));

    hover_notifier_ = std::make_unique<HoverNotifier>(
        this,
        base::BindRepeating(&MenuItemView::OnHover, base::Unretained(this)));
  }
  MenuItemView(const MenuItemView&) = delete;
  MenuItemView& operator=(const MenuItemView&) = delete;
  ~MenuItemView() override = default;

  // views::View:
  int GetHeightForWidth(int w) const override {
    // Make row height fixed avoiding layout manager adjustments.
    return GetPreferredSize().height();
  }

  void OnHover(bool has_hover) {
    if (has_hover && !item_.is_group)
      RequestFocus();
  }

  void OnFocus() override {
    ScrollViewToVisible();
    on_highlight_.Run(false /*by_selection*/);
  }

  const PublicAccountMenuView::Item& item() const { return item_; }

 private:
  void ButtonPressed() {
    if (!item_.is_group)
      on_highlight_.Run(true /*by_selection*/);
  }

  const PublicAccountMenuView::Item item_;
  const PublicAccountMenuView::OnHighlight on_highlight_;
  std::unique_ptr<HoverNotifier> hover_notifier_;
};

class LoginScrollBar : public views::OverlayScrollBar {
 public:
  LoginScrollBar() : OverlayScrollBar(false) {}
  LoginScrollBar(const LoginScrollBar&) = delete;
  LoginScrollBar& operator=(const LoginScrollBar&) = delete;
  ~LoginScrollBar() override = default;

  // OverlayScrollBar:
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Let LoginMenuView to handle up/down keypress.
    return false;
  }
};

}  // namespace

PublicAccountMenuView::TestApi::TestApi(PublicAccountMenuView* view)
    : view_(view) {}

PublicAccountMenuView::TestApi::~TestApi() = default;

views::View* PublicAccountMenuView::TestApi::contents() const {
  return view_->contents_;
}

PublicAccountMenuView::Item::Item() = default;

PublicAccountMenuView::PublicAccountMenuView(const std::vector<Item>& items,
                                             views::View* anchor_view,
                                             LoginButton* opener,
                                             const OnSelect& on_select)
    : LoginBaseBubbleView(anchor_view), opener_(opener), on_select_(on_select) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();

  scroller_ = new views::ScrollView();
  scroller_->SetBackgroundColor(absl::nullopt);
  scroller_->SetDrawOverflowIndicator(false);
  scroller_->ClipHeightTo(kMenuItemHeightDp, kMenuItemHeightDp * 5);
  AddChildView(scroller_);

  views::BoxLayout* box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  box_layout->SetFlexForView(scroller_, 1);

  auto contents = std::make_unique<NonAccessibleView>();
  views::BoxLayout* layout =
      contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->SetDefaultFlex(1);
  layout->set_minimum_cross_axis_size(kMenuItemWidthDp);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  for (size_t i = 0; i < items.size(); i++) {
    const Item& item = items[i];
    contents->AddChildView(new MenuItemView(
        item, base::BindRepeating(&PublicAccountMenuView::OnHighlightChange,
                                  base::Unretained(this), i)));

    if (item.selected)
      selected_index_ = i;
  }
  contents_ = scroller_->SetContents(std::move(contents));
  scroller_->SetVerticalScrollBar(std::make_unique<LoginScrollBar>());
}

PublicAccountMenuView::~PublicAccountMenuView() = default;

void PublicAccountMenuView::OnHighlightChange(size_t item_index,
                                              bool by_selection) {
  selected_index_ = item_index;
  views::View* highlight_item = contents_->children()[item_index];
  for (views::View* child : contents_->GetChildrenInZOrder()) {
    child->SetBackground(views::CreateSolidBackground(
        child == highlight_item ? SK_ColorGRAY : SK_ColorTRANSPARENT));
  }

  if (by_selection) {
    SetVisible(false);
    MenuItemView* menu_view = static_cast<MenuItemView*>(highlight_item);
    on_select_.Run(menu_view->item());
  }
  contents_->SchedulePaint();
}

LoginButton* PublicAccountMenuView::GetBubbleOpener() const {
  return opener_;
}

void PublicAccountMenuView::OnFocus() {
  // Forward the focus to the selected child view.
  contents_->children()[selected_index_]->RequestFocus();
}

bool PublicAccountMenuView::OnKeyPressed(const ui::KeyEvent& event) {
  const ui::KeyboardCode key = event.key_code();
  if (key == ui::VKEY_UP || key == ui::VKEY_DOWN) {
    FindNextItem(key == ui::VKEY_UP)->RequestFocus();
    return true;
  }

  return false;
}

void PublicAccountMenuView::VisibilityChanged(View* starting_from,
                                              bool is_visible) {
  if (is_visible)
    contents_->children()[selected_index_]->RequestFocus();
}

views::View* PublicAccountMenuView::FindNextItem(bool reverse) {
  const auto& children = contents_->children();
  const auto is_item = [](views::View* v) {
    return !static_cast<MenuItemView*>(v)->item().is_group;
  };
  const auto begin = std::next(children.begin(), selected_index_);
  if (reverse) {
    // Subtle: make_reverse_iterator() will result in an iterator that refers to
    // the element before its argument, which is what we want.
    const auto i = std::find_if(std::make_reverse_iterator(begin),
                                children.rend(), is_item);
    return (i == children.rend()) ? *begin : *i;
  }
  const auto i = std::find_if(std::next(begin), children.end(), is_item);
  return (i == children.end()) ? *begin : *i;
}

}  // namespace ash
