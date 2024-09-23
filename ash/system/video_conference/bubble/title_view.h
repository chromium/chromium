// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TITLE_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TITLE_VIEW_H_

#include "ash/style/icon_button.h"
#include "ash/system/video_conference/bubble/mic_indicator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageView;
}

namespace ash::video_conference {

// Part of the vc bubble that holds the icon, title, mic indicator and
// sidetone toggle.
class TitleView : public views::BoxLayoutView {
  METADATA_HEADER(TitleView, views::BoxLayoutView)
 public:
  explicit TitleView(base::OnceClosure close_bubble_callback);
  TitleView(const TitleView&) = delete;
  TitleView& operator=(const TitleView&) = delete;
  ~TitleView() override;

 private:
  raw_ptr<IconButton> sidetone_button_ = nullptr;
};

// This class is used to hold the mic indicator and the sidetone icon.
class MicTestButtonContainer : public views::Button {
  METADATA_HEADER(MicTestButtonContainer, views::Button)
 public:
  explicit MicTestButtonContainer(PressedCallback callback);
  MicTestButtonContainer(const MicTestButtonContainer&) = delete;
  MicTestButtonContainer& operator=(const MicTestButtonContainer&) = delete;
  ~MicTestButtonContainer() override;

 private:
  raw_ptr<views::ImageView> sidetone_icon_;
  raw_ptr<MicIndicator> mic_indicator_;
};

// The mic test button that will be used in the video conference bubble.
// It manages the button's appearance, like its color and size.
class MicTestButton : public views::View {
  METADATA_HEADER(MicTestButton, views::View)
 public:
  explicit MicTestButton();
  MicTestButton(const MicTestButton&) = delete;
  MicTestButton& operator=(const MicTestButton&) = delete;
  ~MicTestButton() override;

 private:
  void OnMicTestButtonClicked(const ui::Event& event);
  void CloseSidetoneBubble();
  void ShowSidetoneBubble(const bool supported);
  // views::View
  void OnThemeChanged() override;

  raw_ptr<views::View> background_view_ = nullptr;
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TITLE_VIEW_H_
