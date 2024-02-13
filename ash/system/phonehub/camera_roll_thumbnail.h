// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/camera_roll_menu_model.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace phonehub {
class CameraRollManager;
class UserActionRecorder;
}  // namespace phonehub

class ASH_EXPORT CameraRollThumbnail : public views::MenuButton,
                                       public views::ContextMenuController {
  METADATA_HEADER(CameraRollThumbnail, views::MenuButton)

 public:
  CameraRollThumbnail(const int index,
                      const phonehub::CameraRollItem& item,
                      phonehub::CameraRollManager* camera_roll_manager,
                      phonehub::UserActionRecorder* user_action_recorder);
  ~CameraRollThumbnail() override;
  CameraRollThumbnail(CameraRollThumbnail&) = delete;
  CameraRollThumbnail operator=(CameraRollThumbnail&) = delete;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::MenuButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ImageThumbnail);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, VideoThumbnail);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest, ImageThumbnail);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest, VideoThumbnail);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest, LeftClickDownload);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest,
                           LeftClickDownloadCantFollowupDownload);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest,
                           LeftClickDownloadWithBackoff);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest, RightClickOpenMenu);
  FRIEND_TEST_ALL_PREFIXES(CameraRollThumbnailTest,
                           ThrottleTimerDoesntBlockRightClickMenu);

  void ButtonPressed();
  ui::SimpleMenuModel* GetMenuModel();
  void DownloadRequested();
  phone_hub_metrics::CameraRollMediaType GetMediaType();

  const int index_;
  const phonehub::proto::CameraRollItemMetadata metadata_;
  const bool video_type_;
  const gfx::ImageSkia image_;

  base::TimeTicks download_throttle_timestamp_ = base::TimeTicks();
  std::unique_ptr<CameraRollMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  raw_ptr<phonehub::CameraRollManager> camera_roll_manager_ = nullptr;
  raw_ptr<phonehub::UserActionRecorder> user_action_recorder_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
