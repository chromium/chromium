// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class VideoConferenceTrayController;

namespace video_conference {

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// The bubble that contains controls for camera and microphone effects,
// and for easy navigation to apps performing video/audio capture.
class ASH_EXPORT BubbleView : public TrayBubbleView {
  METADATA_HEADER(BubbleView, TrayBubbleView)

 public:
  explicit BubbleView(const InitParams& init_params,
                      const MediaApps& media_apps,
                      VideoConferenceTrayController* controller);
  BubbleView(const BubbleView&) = delete;
  BubbleView& operator=(const BubbleView&) = delete;
  ~BubbleView() override;

  // Called when DLC download state updates, used to add and update a warning
  // string.
  void OnDLCDownloadStateInError(
      bool add_warning_view,
      const std::u16string& feature_tile_title_string);

  // views::View:
  void AddedToWidget() override;
  void ChildPreferredSizeChanged(View* child) override;

  // TrayBubbleView:
  bool CanActivate() const override;

  void SetBackgroundReplaceUiVisible(bool visible);

 private:
  // Unowned by `BubbleView`.
  raw_ptr<VideoConferenceTrayController> controller_;

  // String id's of all `FeatureTile`'s that have an error downloading.
  std::set<std::u16string> feature_tile_error_string_ids_;

  const raw_ref<const MediaApps> media_apps_;

  raw_ptr<views::View> set_camera_background_view_ = nullptr;

  base::WeakPtrFactory<BubbleView> weak_factory_{this};
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_
