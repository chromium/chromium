// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "components/global_media_controls/public/constants.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of UnifiedMediaControlsDetailedView in UnifiedSystemTray.
class ASH_EXPORT UnifiedMediaControlsDetailedViewController
    : public DetailedViewController {
 public:
  // If `show_devices_for_item_id` is not empty, when the
  // MediaNotificationListView shows the MediaItemUIView for this ID, it will
  // expand the casting device list too.
  UnifiedMediaControlsDetailedViewController(
      UnifiedSystemTrayController* tray_controller,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      const std::string& show_devices_for_item_id = "");
  ~UnifiedMediaControlsDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  friend class UnifiedMediaControlsDetailedViewControllerTest;

  static bool detailed_view_has_shown_;
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
  const global_media_controls::GlobalMediaControlsEntryPoint entry_point_;
  const std::string show_devices_for_item_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_CONTROLLER_H_
