// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/device_disabled_screen.h"

#include <string>

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"

namespace chromeos {

namespace {
system::DeviceDisablingManager* DeviceDisablingManager() {
  return g_browser_process->platform_part()->device_disabling_manager();
}
}  // namespace

DeviceDisabledScreen::DeviceDisabledScreen(DeviceDisabledScreenView* view)
    : BaseScreen(DeviceDisabledScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DISABLED),
      view_(view) {
  view_->Bind(this);
}

DeviceDisabledScreen::~DeviceDisabledScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void DeviceDisabledScreen::OnViewDestroyed(DeviceDisabledScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void DeviceDisabledScreen::ShowImpl() {
  if (!view_ || !is_hidden())
    return;

  view_->Show(DeviceDisablingManager()->serial_number(),
              DeviceDisablingManager()->enrollment_domain(),
              DeviceDisablingManager()->disabled_message());
  DeviceDisablingManager()->AddObserver(this);
}

void DeviceDisabledScreen::HideImpl() {
  if (is_hidden())
    return;

  if (view_)
    view_->Hide();
  DeviceDisablingManager()->RemoveObserver(this);
}

void DeviceDisabledScreen::OnDisabledMessageChanged(
    const std::string& disabled_message) {
  if (view_)
    view_->UpdateMessage(disabled_message);
}

}  // namespace chromeos
