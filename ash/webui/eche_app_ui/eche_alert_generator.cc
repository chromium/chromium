// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_alert_generator.h"

#include "ash/components/multidevice/logging/logging.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"

namespace ash {
namespace eche_app {

EcheAlertGenerator::EcheAlertGenerator(LaunchAppHelper* launch_app_helper)
    : launch_app_helper_(launch_app_helper) {}

EcheAlertGenerator::~EcheAlertGenerator() = default;

void EcheAlertGenerator::ShowNotification(const std::u16string& title,
                                          const std::u16string& message,
                                          mojom::WebNotificationType type) {
  PA_LOG(INFO) << "echeapi EcheAlertGenerator ShowNotification";
  launch_app_helper_->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI, type));
}

void EcheAlertGenerator::ShowToast(const std::u16string& text) {
  PA_LOG(INFO) << "echeapi EcheAlertGenerator ShowToast";
  launch_app_helper_->ShowToast(text);
}

void EcheAlertGenerator::Bind(
    mojo::PendingReceiver<mojom::NotificationGenerator> receiver) {
  notification_receiver_.reset();
  notification_receiver_.Bind(std::move(receiver));
}

}  // namespace eche_app
}  // namespace ash
