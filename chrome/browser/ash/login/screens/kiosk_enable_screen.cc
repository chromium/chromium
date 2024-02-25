// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/kiosk_enable_screen.h"

#include "base/logging.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_enable_screen_handler.h"

namespace ash {
namespace {

constexpr const char kClose[] = "close";
constexpr const char kEnable[] = "enable";

}  // namespace

KioskEnableScreen::KioskEnableScreen(
    base::WeakPtr<KioskEnableScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(KioskEnableScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

KioskEnableScreen::~KioskEnableScreen() = default;

void KioskEnableScreen::ShowImpl() {
  if (view_)
    view_->Show();
  KioskChromeAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::BindOnce(&KioskEnableScreen::OnGetConsumerKioskAutoLaunchStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskEnableScreen::OnGetConsumerKioskAutoLaunchStatus(
    KioskChromeAppManager::ConsumerKioskAutoLaunchStatus status) {
  is_configurable_ =
      (status ==
       KioskChromeAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable);
  if (!is_configurable_) {
    LOG(WARNING) << "Consumer kiosk auto launch feature is not configurable!";
    HandleClose();
    return;
  }
}

void KioskEnableScreen::HideImpl() {}

void KioskEnableScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kClose)
    HandleClose();
  else if (action_id == kEnable)
    HandleEnable();
  else
    BaseScreen::OnUserAction(args);
}

void KioskEnableScreen::HandleClose() {
  exit_callback_.Run();
}

void KioskEnableScreen::HandleEnable() {
  if (!is_configurable_) {
    NOTREACHED();
    HandleClose();
    return;
  }
  KioskChromeAppManager::Get()->EnableConsumerKioskAutoLaunch(
      base::BindOnce(&KioskEnableScreen::OnEnableConsumerKioskAutoLaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskEnableScreen::OnEnableConsumerKioskAutoLaunch(bool success) {
  if (view_)
    view_->ShowKioskEnabled(success);
  if (!success) {
    LOG(WARNING) << "Consumer kiosk mode can't be enabled!";
  }
}

}  // namespace ash
