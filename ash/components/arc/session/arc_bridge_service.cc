// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_bridge_service.h"

// These header is necessary for instantiation of ConnectionHolder.
#include "ash/components/arc/mojom/adbd.mojom.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/mojom/appfuse.mojom.h"
#include "ash/components/arc/mojom/arc_bridge.mojom.h"
#include "ash/components/arc/mojom/arc_wifi.mojom.h"
#include "ash/components/arc/mojom/audio.mojom.h"
#include "ash/components/arc/mojom/auth.mojom.h"
#include "ash/components/arc/mojom/backup_settings.mojom.h"
#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "ash/components/arc/mojom/boot_phase_monitor.mojom.h"
#include "ash/components/arc/mojom/camera.mojom.h"
#include "ash/components/arc/mojom/crash_collector.mojom.h"
#include "ash/components/arc/mojom/digital_goods.mojom.h"
#include "ash/components/arc/mojom/disk_space.mojom.h"
#include "ash/components/arc/mojom/enterprise_reporting.mojom.h"
#include "ash/components/arc/mojom/error_notification.mojom.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/iio_sensor.mojom.h"
#include "ash/components/arc/mojom/ime.mojom.h"
#include "ash/components/arc/mojom/input_method_manager.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/mojom/keymaster.mojom.h"
#include "ash/components/arc/mojom/keymint.mojom.h"
#include "ash/components/arc/mojom/media_session.mojom.h"
#include "ash/components/arc/mojom/metrics.mojom.h"
#include "ash/components/arc/mojom/midis.mojom.h"
#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/mojom/obb_mounter.mojom.h"
#include "ash/components/arc/mojom/oemcrypto.mojom.h"
#include "ash/components/arc/mojom/on_device_safety.mojom.h"
#include "ash/components/arc/mojom/pip.mojom.h"
#include "ash/components/arc/mojom/policy.mojom.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/mojom/print_spooler.mojom.h"
#include "ash/components/arc/mojom/privacy_items.mojom.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "ash/components/arc/mojom/screen_capture.mojom.h"
#include "ash/components/arc/mojom/sharesheet.mojom.h"
#include "ash/components/arc/mojom/system_state.mojom.h"
#include "ash/components/arc/mojom/timer.mojom.h"
#include "ash/components/arc/mojom/tracing.mojom.h"
#include "ash/components/arc/mojom/tts.mojom.h"
#include "ash/components/arc/mojom/usb_host.mojom.h"
#include "ash/components/arc/mojom/video.mojom.h"
#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/mojom/wake_lock.mojom.h"
#include "ash/components/arc/mojom/wallpaper.mojom.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"

namespace arc {

ArcBridgeService::ArcBridgeService() = default;

ArcBridgeService::~ArcBridgeService() = default;

void ArcBridgeService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::ObserveBeforeArcBridgeClosed() {
  for (auto& observer : observer_list_)
    observer.BeforeArcBridgeClosed();
}

void ArcBridgeService::ObserveAfterArcBridgeClosed() {
  for (auto& observer : observer_list_)
    observer.AfterArcBridgeClosed();
}

}  // namespace arc
