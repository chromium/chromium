// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PRIVACY_INFO_VIEW_H_
#define ASH_APP_LIST_VIEWS_PRIVACY_INFO_VIEW_H_

#include "ash/app_list/views/search_result_base_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace views {
class Button;
class ImageButton;
class ImageView;
class Link;
class StyledLabel;
}  // namespace views

namespace ash {

// View representing privacy info in Launcher.
class PrivacyInfoView : public SearchResultBaseView {
 public:
  PrivacyInfoView(const PrivacyInfoView&) = delete;
  PrivacyInfoView& operator=(const PrivacyInfoView&) = delete;

  ~PrivacyInfoView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // SearchResultBaseView:
  void SelectInitialResultAction(bool reverse_tab_order) override;
  bool SelectNextResultAction(bool reverse_tab_order) override;
  views::View* GetSelectedView() override;

  virtual void LinkClicked() = 0;
  virtual void CloseButtonPressed() = 0;

 protected:
  PrivacyInfoView(int info_string_id, int link_string_id);

 private:
  enum class Action { kNone, kTextLink, kCloseButton };

  // Button pressed callback.
  void OnButtonPressed();

  void InitLayout();
  void InitInfoIcon();
  void InitText();
  void InitCloseButton();

  void UpdateLinkStyle();

  views::ImageView* info_icon_ = nullptr;       // Owned by view hierarchy.
  views::StyledLabel* text_view_ = nullptr;     // Owned by view hierarchy.
  views::ImageButton* close_button_ = nullptr;  // Owned by view hierarchy.

  const int info_string_id_;
  const int link_string_id_;
  views::Link* link_view_ = nullptr;  // Not owned.

  // Indicates which of the privacy notice's actions is selected for keyboard
  // navigation.
  Action selected_action_ = Action::kNone;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PRIVACY_INFO_VIEW_H_
