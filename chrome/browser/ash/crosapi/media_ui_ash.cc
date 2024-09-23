// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/media_ui_ash.h"

#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "components/global_media_controls/public/constants.h"

namespace {
bool IsKioskSession() {
  return ash::Shell::Get()->session_controller()->IsRunningInAppMode();
}
}  // namespace

namespace crosapi {

namespace mojom {
using global_media_controls::mojom::DeviceService;
}  // namespace mojom

MediaUIAsh::MediaUIAsh() = default;
MediaUIAsh::~MediaUIAsh() = default;

void MediaUIAsh::BindReceiver(mojo::PendingReceiver<mojom::MediaUI> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaUIAsh::RegisterDeviceService(
    const base::UnguessableToken& id,
    mojo::PendingRemote<mojom::DeviceService> pending_device_service) {
  mojo::Remote<mojom::DeviceService> device_service{
      std::move(pending_device_service)};
  device_service.set_disconnect_handler(base::BindOnce(
      &MediaUIAsh::RemoveDeviceService, base::Unretained(this), id));
  for (Observer& observer : observers_) {
    observer.OnDeviceServiceRegistered(device_service.get());
  }
  device_services_.emplace(id, std::move(device_service));
}

void MediaUIAsh::ShowDevicePicker(const std::string& item_id) {
  // Keep Media Tray pinned to use a separate widget in kiosk sessions because
  // the Unified System Tray bubble is not available.
  if (IsKioskSession()) {
    ash::MediaTray::SetPinnedToShelf(true);
  }

  if (ash::MediaTray::IsPinnedToShelf()) {
    ash::StatusAreaWidget::ForWindow(ash::Shell::Get()->GetPrimaryRootWindow())
        ->media_tray()
        ->ShowBubbleWithItem(item_id);
  } else {
    ash::UnifiedSystemTray* tray =
        ash::StatusAreaWidget::ForWindow(
            ash::Shell::Get()->GetPrimaryRootWindow())
            ->unified_system_tray();
    tray->ShowBubble();
    tray->bubble()
        ->unified_system_tray_controller()
        ->ShowMediaControlsDetailedView(
            global_media_controls::GlobalMediaControlsEntryPoint::kPresentation,
            item_id);
  }
}

mojom::DeviceService* MediaUIAsh::GetDeviceService(
    const base::UnguessableToken& id) {
  auto service_it = device_services_.find(id);
  return service_it == device_services_.end() ? nullptr
                                              : service_it->second.get();
}

void MediaUIAsh::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MediaUIAsh::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MediaUIAsh::RemoveDeviceService(const base::UnguessableToken& id) {
  device_services_.erase(id);
}

}  // namespace crosapi
