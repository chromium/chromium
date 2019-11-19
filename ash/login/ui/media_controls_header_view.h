// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_
#define ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class ImageButton;
}  // namespace views

namespace ash {

class ASH_EXPORT MediaControlsHeaderView : public views::View,
                                           public views::ButtonListener {
 public:
  explicit MediaControlsHeaderView(base::OnceClosure close_button_cb);
  ~MediaControlsHeaderView() override;

  void SetAppIcon(const gfx::ImageSkia& img);
  void SetAppName(const base::string16& name);

  void SetCloseButtonVisibility(bool visible);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  const base::string16& app_name_for_testing() const;
  const views::ImageView* app_icon_for_testing() const;
  views::ImageButton* close_button_for_testing() const;

 private:
  views::ImageView* app_icon_view_;
  views::Label* app_name_view_;
  views::ImageButton* close_button_ = nullptr;

  base::OnceClosure close_button_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsHeaderView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_
