// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_SCREEN_CAPTURE_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_UNIFIED_SCREEN_CAPTURE_TRAY_ITEM_VIEW_H_

#include <string>

#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/containers/fixed_flat_set.h"
#include "base/memory/weak_ptr.h"

namespace url {
class Origin;
}

namespace ash {

// An indicator shown in UnifiedSystemTray when a web application is using
// screen capturing.
class ASH_EXPORT ScreenCaptureTrayItemView
    : public TrayItemView,
      public MultiCaptureServiceClient::Observer {
 public:
  explicit ScreenCaptureTrayItemView(Shelf* shelf);
  ScreenCaptureTrayItemView(const ScreenCaptureTrayItemView&) = delete;
  ScreenCaptureTrayItemView& operator=(const ScreenCaptureTrayItemView&) =
      delete;
  ~ScreenCaptureTrayItemView() override;

  // views::View:
  const char* GetClassName() const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& point) const override;

  // TrayItemView:
  void HandleLocaleChange() override {}

  // MultiCaptureServiceClient::Observer:
  void MultiCaptureStarted(const std::string& label,
                           const url::Origin& origin) override;
  void MultiCaptureStopped(const std::string& label) override;
  void MultiCaptureServiceClientDestroyed() override;

 private:
  void Refresh();

  base::flat_set<std::string> request_ids_;

  base::ScopedObservation<MultiCaptureServiceClient,
                          MultiCaptureServiceClient::Observer>
      multi_capture_service_client_observation_{this};

  base::WeakPtrFactory<ScreenCaptureTrayItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_SCREEN_CAPTURE_TRAY_ITEM_VIEW_H_
