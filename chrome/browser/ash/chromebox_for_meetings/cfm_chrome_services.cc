// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/cfm_chrome_services.h"

#include "base/command_line.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/data_aggregator_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/browser/cfm_browser_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/device_info/device_info_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/diagnostics/diagnostics_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/external_display_brightness/external_display_brightness_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/logger/cfm_logger_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"

namespace ash::cfm {

void InitializeCfmServices() {
  if (!base::FeatureList::IsEnabled(features::kMojoServices) ||
      !CfmHotlineClient::Get()) {
    return;
  }

  CfmBrowserService::Initialize();
  CfmLoggerService::Initialize();
  DeviceInfoService::Initialize();
  DiagnosticsService::Initialize();
  XuCameraService::Initialize();
  ExternalDisplayBrightnessService::Initialize();
  if (base::FeatureList::IsEnabled(features::kCloudLogger)) {
    DataAggregatorService::Initialize();
  }
}

void ShutdownCfmServices() {
  if (!base::FeatureList::IsEnabled(features::kMojoServices) ||
      !CfmHotlineClient::Get()) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kCloudLogger)) {
    DataAggregatorService::Shutdown();
  }
  ExternalDisplayBrightnessService::Shutdown();
  XuCameraService::Shutdown();
  DiagnosticsService::Shutdown();
  DeviceInfoService::Shutdown();
  CfmLoggerService::Shutdown();
  CfmBrowserService::Shutdown();
}

}  // namespace ash::cfm
