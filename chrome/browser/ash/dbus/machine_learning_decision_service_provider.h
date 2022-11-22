// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_MACHINE_LEARNING_DECISION_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_MACHINE_LEARNING_DECISION_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

namespace power {
namespace ml {
class UserActivityController;
}  // namespace ml
}  // namespace power

// This class processes machine learning decision requests from Chrome OS side.
//
// ShouldDeferScreenDim:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.MlDecisionService
//     /org/chromium/MlDecisionService
//     org.chromium.MlDecisionService.ShouldDeferScreenDim
//     boolean: true or false
//
// % (True means smart dim decides to defer the imminent screen dimming.)
//
// Now it only exports ShouldDeferScreenDim for powerd. New machine learning
// related methods can be added when required.
class MachineLearningDecisionServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  MachineLearningDecisionServiceProvider();

  MachineLearningDecisionServiceProvider(
      const MachineLearningDecisionServiceProvider&) = delete;
  MachineLearningDecisionServiceProvider& operator=(
      const MachineLearningDecisionServiceProvider&) = delete;

  ~MachineLearningDecisionServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to D-Bus requests.
  void ShouldDeferScreenDim(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Sends |defer_dimming| as the response to a ShouldDeferScreenDim method
  // call.
  void SendSmartDimDecision(
      std::unique_ptr<dbus::Response> response,
      dbus::ExportedObject::ResponseSender response_sender,
      bool defer_dimming);

  // The real provider of ShouldDeferScreenDim
  std::unique_ptr<power::ml::UserActivityController> user_activity_controller_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<MachineLearningDecisionServiceProvider>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_MACHINE_LEARNING_DECISION_SERVICE_PROVIDER_H_
