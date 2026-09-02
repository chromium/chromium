// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/device_disabled_screen.h"

#include <string>

#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

system::DeviceDisablingManager* DeviceDisablingManager() {
  return g_browser_process->platform_part()->device_disabling_manager();
}

}  // namespace

DeviceDisabledScreen::DeviceDisabledScreen(
    policy::DeviceRestrictionScheduleController*
        device_restriction_schedule_controller,
    base::WeakPtr<DeviceDisabledScreenView> view)
    : BaseScreen(DeviceDisabledScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DISABLED),
      device_restriction_schedule_controller_(
          CHECK_DEREF(device_restriction_schedule_controller)),
      view_(std::move(view)) {}

DeviceDisabledScreen::~DeviceDisabledScreen() = default;

void DeviceDisabledScreen::ShowImpl() {
  if (!view_ || !is_hidden()) {
    return;
  }

  DeviceDisabledScreenView::Params params;
  params.serial = DeviceDisablingManager()->serial_number();
  params.domain = DeviceDisablingManager()->enrollment_domain();
  params.message = DeviceDisablingManager()->disabled_message();
  params.location_tracking_enabled =
      DeviceDisablingManager()->location_tracking_enabled();
  params.device_restriction_schedule_enabled =
      device_restriction_schedule_controller_->RestrictionScheduleEnabled();
  params.device_name = ui::GetChromeOSDeviceName();
  params.restriction_schedule_end_day =
      device_restriction_schedule_controller_->RestrictionScheduleEndDay();
  params.restriction_schedule_end_time =
      device_restriction_schedule_controller_->RestrictionScheduleEndTime();
  view_->Show(params);
  observation_.Observe(DeviceDisablingManager());
}

void DeviceDisabledScreen::HideImpl() {
  if (is_hidden()) {
    return;
  }

  NOTREACHED() << "Device disabled screen can't be hidden";
}

void DeviceDisabledScreen::OnDisabledMessageChanged(
    const std::string& disabled_message) {
  if (view_) {
    view_->UpdateMessage(disabled_message);
  }
}

void DeviceDisabledScreen::OnLocationTrackingEnabledChanged(
    bool location_tracking_enabled) {
  if (view_) {
    view_->UpdateLocationTracking(location_tracking_enabled);
  }
}

void DeviceDisabledScreen::OnRestrictionScheduleMessageChanged() {
  if (view_) {
    view_->UpdateRestrictionScheduleMessage(
        device_restriction_schedule_controller_->RestrictionScheduleEndDay(),
        device_restriction_schedule_controller_->RestrictionScheduleEndTime());
  }
}

}  // namespace ash
