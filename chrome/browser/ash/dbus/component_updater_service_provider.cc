// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/component_updater_service_provider.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorInternalError[] = "org.freedesktop.DBus.Error.InternalError";

std::string ErrorToString(component_updater::ComponentManagerAsh::Error error) {
  switch (error) {
    case component_updater::ComponentManagerAsh::Error::NONE:
      return "NONE";
    case component_updater::ComponentManagerAsh::Error::UNKNOWN_COMPONENT:
      return "UNKNOWN_COMPONENT";
    case component_updater::ComponentManagerAsh::Error::INSTALL_FAILURE:
      return "INSTALL_FAILURE";
    case component_updater::ComponentManagerAsh::Error::MOUNT_FAILURE:
      return "MOUNT_FAILURE";
    case component_updater::ComponentManagerAsh::Error::
        COMPATIBILITY_CHECK_FAILED:
      return "COMPATIBILITY_CHECK_FAILED";
    case component_updater::ComponentManagerAsh::Error::NOT_FOUND:
      return "NOT_FOUND";
    case component_updater::ComponentManagerAsh::Error::UPDATE_IN_PROGRESS:
      return "UPDATE_IN_PROGRESS";
  }
  return "Unknown error code";
}

}  // namespace

ComponentUpdaterServiceProvider::ComponentUpdaterServiceProvider(
    component_updater::ComponentManagerAsh* cros_component_manager) {
  DCHECK(cros_component_manager);

  cros_component_manager_ = cros_component_manager;
  cros_component_manager_->SetDelegate(this);
}

ComponentUpdaterServiceProvider::~ComponentUpdaterServiceProvider() {
  cros_component_manager_->SetDelegate(nullptr);
}

void ComponentUpdaterServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kComponentUpdaterServiceInterface,
      chromeos::kComponentUpdaterServiceLoadComponentMethod,
      base::BindRepeating(&ComponentUpdaterServiceProvider::LoadComponent,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ComponentUpdaterServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      chromeos::kComponentUpdaterServiceInterface,
      chromeos::kComponentUpdaterServiceUnloadComponentMethod,
      base::BindRepeating(&ComponentUpdaterServiceProvider::UnloadComponent,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ComponentUpdaterServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object_ = exported_object;
}

void ComponentUpdaterServiceProvider::EmitInstalledSignal(
    const std::string& component) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ComponentUpdaterServiceProvider::EmitInstalledSignalInternal,
          weak_ptr_factory_.GetWeakPtr(), component));
}

void ComponentUpdaterServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void ComponentUpdaterServiceProvider::LoadComponent(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string component_name;
  // |mount| is an optional parameter, and by default is true.
  bool mount = true;
  if (reader.PopString(&component_name)) {
    // dbus::MessageReader::PopBool sets its out-param to false on failure.
    // Resets |mount| to its default value on failure.
    if (!reader.PopBool(&mount))
      mount = true;
    cros_component_manager_->Load(
        component_name,
        mount ? component_updater::ComponentManagerAsh::MountPolicy::kMount
              : component_updater::ComponentManagerAsh::MountPolicy::kDontMount,
        component_updater::ComponentManagerAsh::UpdatePolicy::kDontForce,
        base::BindOnce(&ComponentUpdaterServiceProvider::OnLoadComponent,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender)));
  } else {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidArgs,
            "Need a string and a boolean parameter.");
    std::move(response_sender).Run(std::move(error_response));
  }
}

void ComponentUpdaterServiceProvider::OnLoadComponent(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    component_updater::ComponentManagerAsh::Error error,
    const base::FilePath& result) {
  if (error != component_updater::ComponentManagerAsh::Error::NONE) {
    LOG(ERROR) << "Component updater Load API error: " << ErrorToString(error);
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(result.value());
  std::move(response_sender).Run(std::move(response));
}

void ComponentUpdaterServiceProvider::UnloadComponent(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string component_name;
  if (reader.PopString(&component_name)) {
    if (cros_component_manager_->Unload(component_name)) {
      std::move(response_sender)
          .Run(dbus::Response::FromMethodCall(method_call));
    } else {
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInternalError, "Failed to unload component"));
    }
  } else {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidArgs,
            "Missing component name string argument."));
  }
}

void ComponentUpdaterServiceProvider::EmitInstalledSignalInternal(
    const std::string& component) {
  DCHECK(exported_object_);

  dbus::Signal signal(
      chromeos::kComponentUpdaterServiceInterface,
      chromeos::kComponentUpdaterServiceComponentInstalledSignal);

  dbus::MessageWriter writer(&signal);
  writer.AppendString(component);
  exported_object_->SendSignal(&signal);
}

}  // namespace ash
