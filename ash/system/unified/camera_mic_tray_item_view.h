// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CAMERA_MIC_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CAMERA_MIC_TRAY_ITEM_VIEW_H_

#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"

namespace ash {

// An indicator shown in UnifiedSystemTray when a VM is accessing the camera or
// mic. We might want to extend this feature to the browser in the future.
class ASH_EXPORT CameraMicTrayItemView : public TrayItemView,
                                         public SessionObserver,
                                         public MediaCaptureObserver {
 public:
  enum class Type {
    kCamera,
    kMic,
  };

  CameraMicTrayItemView(Shelf* shelf, Type type);
  ~CameraMicTrayItemView() override;

  CameraMicTrayItemView(const CameraMicTrayItemView&) = delete;
  CameraMicTrayItemView& operator=(const CameraMicTrayItemView&) = delete;

  std::u16string GetAccessibleNameString() const;

  // views::View:
  const char* GetClassName() const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TrayItemView:
  void HandleLocaleChange() override;

  // MediaCaptureObserver:
  void OnVmMediaNotificationChanged(bool camera,
                                    bool mic,
                                    bool camera_and_mic) override;

 private:
  void Update();
  void FetchMessage();

  const Type type_;
  bool active_ = false;
  bool with_mic_ = false;  // Only for `type_ == kCamera`.
  bool is_primary_session_ = false;
  std::u16string message_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CAMERA_MIC_TRAY_ITEM_VIEW_H_
