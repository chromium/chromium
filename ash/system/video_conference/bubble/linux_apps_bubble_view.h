// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_LINUX_APPS_BUBBLE_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_LINUX_APPS_BUBBLE_VIEW_H_

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash::video_conference {

using MediaApps = VideoConferenceManagerBase::MediaApps;

// The bubble view that shows for VC panel when all running media apps are Linux
// apps. In this case we just display a smaller bubble that contains no effects.
class LinuxAppsBubbleView : public TrayBubbleView {
  METADATA_HEADER(LinuxAppsBubbleView, TrayBubbleView)

 public:
  LinuxAppsBubbleView(const InitParams& init_params, const MediaApps& apps);

  LinuxAppsBubbleView(const LinuxAppsBubbleView&) = delete;
  LinuxAppsBubbleView& operator=(const LinuxAppsBubbleView&) = delete;

  ~LinuxAppsBubbleView() override = default;

  // TrayBubbleView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_LINUX_APPS_BUBBLE_VIEW_H_
