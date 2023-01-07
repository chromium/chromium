// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/machine_learning_decision_service_provider.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/power/ml/user_activity_controller.h"
#include "chrome/common/chrome_features.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

MachineLearningDecisionServiceProvider::MachineLearningDecisionServiceProvider()
    : user_activity_controller_(
          std::make_unique<power::ml::UserActivityController>()) {}

MachineLearningDecisionServiceProvider::
    ~MachineLearningDecisionServiceProvider() = default;

void MachineLearningDecisionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kMlDecisionServiceInterface,
      chromeos::kMlDecisionServiceShouldDeferScreenDimMethod,
      base::BindRepeating(
          &MachineLearningDecisionServiceProvider::ShouldDeferScreenDim,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MachineLearningDecisionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MachineLearningDecisionServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void MachineLearningDecisionServiceProvider::ShouldDeferScreenDim(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  // Smart dim only works when features::kSmartDim is enabled. Simply return
  // false otherwise.
  if (!base::FeatureList::IsEnabled(features::kSmartDim)) {
    SendSmartDimDecision(std::move(response), std::move(response_sender),
                         false);
    return;
  }

  user_activity_controller_->ShouldDeferScreenDim(base::BindOnce(
      &MachineLearningDecisionServiceProvider::SendSmartDimDecision,
      weak_ptr_factory_.GetWeakPtr(), std::move(response),
      std::move(response_sender)));
}

void MachineLearningDecisionServiceProvider::SendSmartDimDecision(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    bool defer_dimming) {
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(defer_dimming);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
