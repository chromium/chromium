// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/crosapi/browser_manager.h"
#include "chrome/browser/chromeos/crosapi/feedback_ash.h"
#include "chrome/browser/chromeos/crosapi/keystore_service_ash.h"
#include "chrome/browser/chromeos/crosapi/message_center_ash.h"
#include "chrome/browser/chromeos/crosapi/screen_manager_ash.h"
#include "chrome/browser/chromeos/crosapi/select_file_ash.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "content/public/browser/device_service.h"

namespace crosapi {

AshChromeServiceImpl::AshChromeServiceImpl(
    mojo::PendingReceiver<mojom::AshChromeService> pending_receiver)
    : receiver_(this, std::move(pending_receiver)),
      screen_manager_ash_(std::make_unique<ScreenManagerAsh>()) {
  // TODO(hidehiko): Remove non-critical log from here.
  // Currently this is the signal that the connection is established.
  LOG(WARNING) << "AshChromeService connected.";
}

AshChromeServiceImpl::~AshChromeServiceImpl() = default;

void AshChromeServiceImpl::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  keystore_service_ash_ =
      std::make_unique<crosapi::KeystoreServiceAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindMessageCenter(
    mojo::PendingReceiver<mojom::MessageCenter> receiver) {
  message_center_ash_ = std::make_unique<MessageCenterAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindSelectFile(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  select_file_ash_ = std::make_unique<SelectFileAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_ash_->BindReceiver(std::move(receiver));
}

void AshChromeServiceImpl::BindHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  content::GetDeviceService().BindHidManager(std::move(receiver));
}

void AshChromeServiceImpl::BindFeedback(
    mojo::PendingReceiver<mojom::Feedback> receiver) {
  feedback_ash_ = std::make_unique<FeedbackAsh>(std::move(receiver));
}

void AshChromeServiceImpl::OnLacrosStartup(mojom::LacrosInfoPtr lacros_info) {
  BrowserManager::Get()->set_lacros_version(lacros_info->lacros_version);
}

}  // namespace crosapi
