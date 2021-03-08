// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/kiosk_enable_screen.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"

namespace chromeos {
namespace {

constexpr const char kClose[] = "close";
constexpr const char kEnable[] = "enable";

}  // namespace

KioskEnableScreen::KioskEnableScreen(
    KioskEnableScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(KioskEnableScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetScreen(this);
}

KioskEnableScreen::~KioskEnableScreen() {
  if (view_)
    view_->SetScreen(nullptr);
}

void KioskEnableScreen::OnViewDestroyed(KioskEnableScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void KioskEnableScreen::ShowImpl() {
  if (view_)
    view_->Show();
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::BindOnce(&KioskEnableScreen::OnGetConsumerKioskAutoLaunchStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskEnableScreen::OnGetConsumerKioskAutoLaunchStatus(
    KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  is_configurable_ =
      (status == KioskAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable);
  if (!is_configurable_) {
    LOG(WARNING) << "Consumer kiosk auto launch feature is not configurable!";
    HandleClose();
    return;
  }
}

void KioskEnableScreen::HideImpl() {}

void KioskEnableScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kClose)
    HandleClose();
  else if (action_id == kEnable)
    HandleEnable();
  else
    BaseScreen::OnUserAction(action_id);
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
  KioskAppManager::Get()->EnableConsumerKioskAutoLaunch(
      base::BindOnce(&KioskEnableScreen::OnEnableConsumerKioskAutoLaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskEnableScreen::OnEnableConsumerKioskAutoLaunch(bool success) {
  view_->ShowKioskEnabled(success);
  if (!success) {
    LOG(WARNING) << "Consumer kiosk mode can't be enabled!";
  }
}

}  // namespace chromeos
