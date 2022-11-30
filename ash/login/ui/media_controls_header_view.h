// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_
#define ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ui {
class ImageModel;
}

namespace views {
class ImageView;
class Label;
class ImageButton;
}  // namespace views

namespace ash {

class ASH_EXPORT MediaControlsHeaderView : public views::View,
                                           views::ViewObserver {
 public:
  explicit MediaControlsHeaderView(
      views::Button::PressedCallback close_button_cb);

  MediaControlsHeaderView(const MediaControlsHeaderView&) = delete;
  MediaControlsHeaderView& operator=(const MediaControlsHeaderView&) = delete;

  ~MediaControlsHeaderView() override;

  void SetAppIcon(const ui::ImageModel& img);
  void SetAppName(const std::u16string& name);

  void SetForceShowCloseButton(bool force_visible);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ViewObserver:
  void OnViewFocused(views::View*) override;
  void OnViewBlurred(views::View*) override;

  const std::u16string& app_name_for_testing() const;
  const views::ImageView* app_icon_for_testing() const;
  views::ImageButton* close_button_for_testing() const;

 private:
  void UpdateCloseButtonVisibility();

  views::ImageView* app_icon_view_;
  views::Label* app_name_view_;
  views::ImageButton* close_button_ = nullptr;

  bool force_close_x_visible_ = false;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_
