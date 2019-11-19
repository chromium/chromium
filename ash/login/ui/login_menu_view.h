// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_MENU_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_button.h"
#include "base/callback.h"
#include "ui/views/view.h"

namespace ash {

// Implements a menu view for login screen.
// This is used instead of views::Combobox because we want a more customisable
// view for the menu items to support some sepcific styles like nested menu
// entries which have different left margin (less than regular items) and are
// not selectable. views::Combobox uses views::MenuItemView which is not very
// straightforward to customize.
class ASH_EXPORT LoginMenuView : public LoginBaseBubbleView {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginMenuView* view);
    ~TestApi();

    views::View* contents() const;

   private:
    LoginMenuView* const view_;
  };

  struct Item {
    Item();

    std::string title;
    std::string value;
    bool is_group = false;
    bool selected = false;
  };

  using OnSelect = base::RepeatingCallback<void(Item item)>;
  using OnHighlight = base::RepeatingCallback<void(bool by_selection)>;

  LoginMenuView(const std::vector<Item>& items,
                views::View* anchor_view,
                LoginButton* opener_,
                const OnSelect& on_select);
  ~LoginMenuView() override;

  void OnHighlightChange(size_t item_index, bool by_selection);

  // LoginBaseBubbleView:
  LoginButton* GetBubbleOpener() const override;

  // views::View:
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

 private:
  views::View* FindNextItem(bool reverse);

  // Owned by this class.
  views::ScrollView* scroller_ = nullptr;

  // Owned by ScrollView.
  views::View* contents_ = nullptr;

  LoginButton* opener_ = nullptr;

  const OnSelect on_select_;
  size_t selected_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LoginMenuView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_MENU_VIEW_H_
