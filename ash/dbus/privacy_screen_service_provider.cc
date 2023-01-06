// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/privacy_screen_service_provider.h"

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

privacy_screen::PrivacyScreenSetting_PrivacyScreenState
GetPrivacyScreenState() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  DCHECK(privacy_screen_controller);
  if (!privacy_screen_controller->IsSupported()) {
    return privacy_screen::
        PrivacyScreenSetting_PrivacyScreenState_NOT_SUPPORTED;
  }
  if (privacy_screen_controller->GetEnabled())
    return privacy_screen::PrivacyScreenSetting_PrivacyScreenState_ENABLED;
  return privacy_screen::PrivacyScreenSetting_PrivacyScreenState_DISABLED;
}

}  // namespace

PrivacyScreenServiceProvider::PrivacyScreenServiceProvider() = default;

PrivacyScreenServiceProvider::~PrivacyScreenServiceProvider() {
  DCHECK(Shell::Get() && Shell::Get()->privacy_screen_controller());
}

void PrivacyScreenServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object_ = exported_object;

  exported_object_->ExportMethod(
      privacy_screen::kPrivacyScreenServiceInterface,
      privacy_screen::kPrivacyScreenServiceGetPrivacyScreenSettingMethod,
      base::BindRepeating(
          &PrivacyScreenServiceProvider::GetPrivacyScreenSetting,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PrivacyScreenServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  DCHECK(privacy_screen_controller);
  privacy_screen_observation_.Observe(privacy_screen_controller);
}

void PrivacyScreenServiceProvider::OnExported(const std::string& interface_name,
                                              const std::string& method_name,
                                              bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void PrivacyScreenServiceProvider::GetPrivacyScreenSetting(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  state_ = GetPrivacyScreenState();
  privacy_screen::PrivacyScreenSetting setting;
  setting.set_state(state_);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(setting);
  std::move(response_sender).Run(std::move(response));
}

void PrivacyScreenServiceProvider::OnPrivacyScreenSettingChanged(
    bool enabled,
    bool notify_ui) {
  DCHECK(exported_object_);

  const privacy_screen::PrivacyScreenSetting_PrivacyScreenState new_state =
      GetPrivacyScreenState();
  if (new_state == state_)
    return;

  state_ = new_state;
  privacy_screen::PrivacyScreenSetting setting;
  setting.set_state(state_);

  dbus::Signal signal(
      privacy_screen::kPrivacyScreenServiceInterface,
      privacy_screen::kPrivacyScreenServicePrivacyScreenSettingChangedSignal);
  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(setting);
  exported_object_->SendSignal(&signal);
}

}  // namespace ash
