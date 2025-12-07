// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/display_service_provider.h"

#include <utility>

#include "ash/shell.h"
#include "ash/wm/screen_dimmer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/trace_event/trace_event.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {
namespace {

void OnDisplayOwnershipChanged(
    dbus::ExportedObject::ResponseSender response_sender,
    std::unique_ptr<dbus::Response> response,
    bool status) {
  TRACE_EVENT1("ui", "OnDisplayOwnershipChanged", "status", status);

  dbus::MessageWriter writer(response.get());
  writer.AppendBool(status);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace

class DisplayServiceProvider::Impl {
 public:
  Impl() = default;

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl() = default;

  void SetDimming(bool dimmed);
  void TakeDisplayOwnership(base::OnceCallback<void(bool)> callback);
  void ReleaseDisplayOwnership(base::OnceCallback<void(bool)> callback);

 private:
  std::unique_ptr<ScreenDimmer> screen_dimmer_;
};

void DisplayServiceProvider::Impl::SetDimming(bool dimmed) {
  if (!screen_dimmer_) {
    screen_dimmer_ = std::make_unique<ScreenDimmer>();
  }
  screen_dimmer_->SetDimming(dimmed);
}

void DisplayServiceProvider::Impl::TakeDisplayOwnership(
    base::OnceCallback<void(bool)> callback) {
  if (!Shell::Get()->display_configurator()) {
    LOG(ERROR) << "Display Controller not connected";
    std::move(callback).Run(false);
    return;
  }
  Shell::Get()->display_configurator()->TakeControl(std::move(callback));
}

void DisplayServiceProvider::Impl::ReleaseDisplayOwnership(
    base::OnceCallback<void(bool)> callback) {
  if (!Shell::Get()->display_configurator()) {
    LOG(ERROR) << "Display Controller not connected";
    std::move(callback).Run(false);
    return;
  }
  Shell::Get()->display_configurator()->RelinquishControl(std::move(callback));
}

DisplayServiceProvider::DisplayServiceProvider()
    : impl_(std::make_unique<Impl>()) {}

DisplayServiceProvider::~DisplayServiceProvider() = default;

void DisplayServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kDisplayServiceInterface,
      chromeos::kDisplayServiceSetPowerMethod,
      base::BindRepeating(&DisplayServiceProvider::SetDisplayPower,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DisplayServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      chromeos::kDisplayServiceInterface,
      chromeos::kDisplayServiceSetSoftwareDimmingMethod,
      base::BindRepeating(&DisplayServiceProvider::SetDisplaySoftwareDimming,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DisplayServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      chromeos::kDisplayServiceInterface,
      chromeos::kDisplayServiceTakeOwnershipMethod,
      base::BindRepeating(&DisplayServiceProvider::TakeDisplayOwnership,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DisplayServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      chromeos::kDisplayServiceInterface,
      chromeos::kDisplayServiceReleaseOwnershipMethod,
      base::BindRepeating(&DisplayServiceProvider::ReleaseDisplayOwnership,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DisplayServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisplayServiceProvider::SetDisplayPower(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  int int_state = 0;
  if (!reader.PopInt32(&int_state)) {
    LOG(ERROR) << "Unable to parse request: "
               << chromeos::kDisplayServiceSetPowerMethod;
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
    return;
  }

  // Turning displays off when the device becomes idle or on just before
  // we suspend may trigger a mouse move, which would then be incorrectly
  // reported as user activity.  Let the UserActivityDetector
  // know so that it can ignore such events.
  ui::UserActivityDetector::Get()->OnDisplayPowerChanging();

  Shell::Get()->display_configurator()->SetDisplayPower(
      static_cast<chromeos::DisplayPowerState>(int_state),
      display::DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::BindOnce(
          [](dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender,
             bool /*status*/) {
            std::move(response_sender)
                .Run(dbus::Response::FromMethodCall(method_call));
          },
          method_call, std::move(response_sender)));
}

void DisplayServiceProvider::SetDisplaySoftwareDimming(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  bool dimmed = false;
  if (reader.PopBool(&dimmed)) {
    impl_->SetDimming(dimmed);
  } else {
    LOG(ERROR) << "Unable to parse request: "
               << chromeos::kDisplayServiceSetSoftwareDimmingMethod;
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void DisplayServiceProvider::TakeDisplayOwnership(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  TRACE_EVENT0("ui", "DisplayServiceProvider::TakeDisplayOwnership");
  impl_->TakeDisplayOwnership(
      base::BindOnce(&OnDisplayOwnershipChanged, std::move(response_sender),
                     dbus::Response::FromMethodCall(method_call)));
}

void DisplayServiceProvider::ReleaseDisplayOwnership(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  TRACE_EVENT0("ui", "DisplayServiceProvider::ReleaseDisplayOwnership");
  impl_->ReleaseDisplayOwnership(
      base::BindOnce(&OnDisplayOwnershipChanged, std::move(response_sender),
                     dbus::Response::FromMethodCall(method_call)));
}

void DisplayServiceProvider::OnExported(const std::string& interface_name,
                                        const std::string& method_name,
                                        bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

}  // namespace ash
