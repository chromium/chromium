// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"

namespace chromeos {

namespace {
system::DeviceDisablingManager* DeviceDisablingManager() {
  return g_browser_process->platform_part()->device_disabling_manager();
}
}  // namespace

DeviceDisabledScreen::DeviceDisabledScreen(DeviceDisabledScreenView* view)
    : BaseScreen(DeviceDisabledScreenView::kScreenId), view_(view) {
  view_->SetDelegate(this);
}

DeviceDisabledScreen::~DeviceDisabledScreen() {
  if (view_)
    view_->SetDelegate(nullptr);
}

void DeviceDisabledScreen::OnViewDestroyed(DeviceDisabledScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

const std::string& DeviceDisabledScreen::GetEnrollmentDomain() const {
  return DeviceDisablingManager()->enrollment_domain();
}

const std::string& DeviceDisabledScreen::GetMessage() const {
  return DeviceDisablingManager()->disabled_message();
}

const std::string& DeviceDisabledScreen::GetSerialNumber() const {
  return DeviceDisablingManager()->serial_number();
}

void DeviceDisabledScreen::Show() {
  if (!view_ || showing_)
    return;

  showing_ = true;
  view_->Show();
  DeviceDisablingManager()->AddObserver(this);
  if (!DeviceDisablingManager()->disabled_message().empty())
    view_->UpdateMessage(DeviceDisablingManager()->disabled_message());
}

void DeviceDisabledScreen::Hide() {
  if (!showing_)
    return;
  showing_ = false;

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
