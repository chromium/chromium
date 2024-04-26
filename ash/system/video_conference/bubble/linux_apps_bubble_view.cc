// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/linux_apps_bubble_view.h"

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_button_base.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash::video_conference {

namespace {

constexpr int kLinuxReturnToAppPanelVerticalPadding = 8;
constexpr int kLinuxReturnToAppPanelHorizontalPadding = 16;
constexpr int kLinuxReturnToAppPanelSpacing = 8;
constexpr int kLinuxReturnToAppButtonSpacing = 12;

// The return to app button that is used for Linux apps displayed in
// `LinuxAppsBubbleView` with customized spacing/padding.
class LinuxReturnToAppButton : public ReturnToAppButtonBase {
  METADATA_HEADER(LinuxReturnToAppButton, ReturnToAppButtonBase)

 public:
  explicit LinuxReturnToAppButton(
      const crosapi::mojom::VideoConferenceMediaAppInfoPtr& app)
      : ReturnToAppButtonBase(
            app->id,
            app->is_capturing_camera,
            app->is_capturing_microphone,
            app->is_capturing_screen,
            video_conference_utils::GetMediaAppDisplayText(app),
            app->app_type) {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetDefault(views::kMarginsKey,
                    gfx::Insets::TLBR(0, kLinuxReturnToAppButtonSpacing / 2, 0,
                                      kLinuxReturnToAppButtonSpacing / 2));
  }

  LinuxReturnToAppButton(const LinuxReturnToAppButton&) = delete;
  LinuxReturnToAppButton& operator=(const LinuxReturnToAppButton&) = delete;

  ~LinuxReturnToAppButton() override = default;
};

BEGIN_METADATA(LinuxReturnToAppButton)
END_METADATA

}  // namespace

LinuxAppsBubbleView::LinuxAppsBubbleView(const InitParams& init_params,
                                         const MediaApps& apps)
    : TrayBubbleView(init_params) {
  SetID(BubbleViewID::kLinuxAppBubbleView);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(
          gfx::Insets::VH(kLinuxReturnToAppPanelVerticalPadding -
                              kLinuxReturnToAppPanelSpacing / 2,
                          kLinuxReturnToAppPanelHorizontalPadding -
                              kLinuxReturnToAppButtonSpacing / 2))
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(kLinuxReturnToAppPanelSpacing / 2, 0,
                                    kLinuxReturnToAppPanelSpacing / 2, 0));

  for (auto& app : apps) {
    AddChildView(std::make_unique<LinuxReturnToAppButton>(app));
  }
}

gfx::Size LinuxAppsBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // We don't want the fixed width configuration of
  // `TrayBubbleView::CalculatePreferredSize()`. We will just use the default
  // method and let the width be dynamically adjusted based on children's width.
  return views::View::CalculatePreferredSize(available_size);
}

BEGIN_METADATA(LinuxAppsBubbleView);
END_METADATA

}  // namespace ash::video_conference
