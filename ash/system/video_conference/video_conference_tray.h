// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
class Widget;
}  // namespace views

namespace ash {

class IconButton;
class Shelf;
class TrayBubbleView;
class TrayBubbleWrapper;

// This class represents the VC Controls tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT VideoConferenceTray : public TrayBackgroundView {
 public:
  METADATA_HEADER(VideoConferenceTray);

  explicit VideoConferenceTray(Shelf* shelf);
  VideoConferenceTray(const VideoConferenceTray&) = delete;
  VideoConferenceTray& operator=(const VideoConferenceTray&) = delete;
  ~VideoConferenceTray() override;

  // TrayBackgroundView:
  void CloseBubble() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  std::u16string GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void HandleLocaleChange() override;
  void UpdateLayout() override;
  void OnThemeChanged() override;
  void UpdateAfterLoginStatusChange() override;

 private:
  friend class VideoConferenceTrayTest;

  // Updates the orientation of the expand indicator, based on shelf alignment
  // and whether the bubble is opened.
  void UpdateExpandIndicator();

  // Owned by the views hierarchy.
  IconButton* audio_icon_ = nullptr;
  IconButton* camera_icon_ = nullptr;
  IconButton* screen_share_icon_ = nullptr;
  views::ImageView* expand_indicator_ = nullptr;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_
