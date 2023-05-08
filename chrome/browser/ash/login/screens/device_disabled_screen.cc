// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/device_disabled_screen.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"

namespace ash {
namespace {

system::DeviceDisablingManager* DeviceDisablingManager() {
  return g_browser_process->platform_part()->device_disabling_manager();
}

}  // namespace

DeviceDisabledScreen::DeviceDisabledScreen(
    base::WeakPtr<DeviceDisabledScreenView> view)
    : BaseScreen(DeviceDisabledScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DISABLED),
      view_(std::move(view)) {}

DeviceDisabledScreen::~DeviceDisabledScreen() = default;

void DeviceDisabledScreen::ShowImpl() {
  if (!view_ || !is_hidden()) {
    return;
  }

  view_->Show(
      DeviceDisablingManager()->serial_number(),
      DeviceDisablingManager()->enrollment_domain(),
      DeviceDisablingManager()->disabled_message(),
      // TODO() remove this parameter from DeviceDisabledScreenHandler::Show.
      /*is_disabled_ad_device=*/false);
  DeviceDisablingManager()->AddObserver(this);
}

void DeviceDisabledScreen::HideImpl() {
  if (is_hidden()) {
    return;
  }

  NOTREACHED() << "Device disabled screen can't be hidden";
  DeviceDisablingManager()->RemoveObserver(this);
}

void DeviceDisabledScreen::OnDisabledMessageChanged(
    const std::string& disabled_message) {
  if (view_) {
    view_->UpdateMessage(disabled_message);
  }
}

}  // namespace ash
