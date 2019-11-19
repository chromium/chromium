// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_PRIVACY_INFO_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_PRIVACY_INFO_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class StyledLabel;
}  // namespace views

namespace ash {

class AppListViewDelegate;
class SearchResultPageView;

// View representing the Assistant privacy info in Launcher.
class PrivacyInfoView : public views::View,
                        public views::ButtonListener,
                        public views::StyledLabelListener {
 public:
  PrivacyInfoView(AppListViewDelegate* view_delegate,
                  SearchResultPageView* search_result_page_view);
  ~PrivacyInfoView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

 private:
  void InitLayout();
  void InitInfoIcon();
  void InitText();
  void InitCloseButton();

  views::View* row_container_ = nullptr;        // Owned by view hierarchy.
  views::ImageView* info_icon_ = nullptr;       // Owned by view hierarchy.
  views::StyledLabel* text_view_ = nullptr;     // Owned by view hierarchy.
  views::ImageButton* close_button_ = nullptr;  // Owned by view hierarchy.

  AppListViewDelegate* const view_delegate_;
  SearchResultPageView* const search_result_page_view_;

  DISALLOW_COPY_AND_ASSIGN(PrivacyInfoView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_PRIVACY_INFO_VIEW_H_
