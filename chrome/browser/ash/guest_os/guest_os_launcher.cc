// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_launcher.h"

#include <sstream>

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"

namespace guest_os::launcher {

namespace {

ResponseType Success(std::string vm_name, std::string container_name) {
  vm_tools::launch::EnsureVmLaunchedResponse response;
  response.set_vm_name(std::move(vm_name));
  response.set_container_name(std::move(container_name));
  return ResponseType(std::move(response));
}

void LaunchBorealis(Profile* profile, LaunchCallback callback) {
  borealis::BorealisService::GetForProfile(profile)
      ->ContextManager()
      .StartBorealis(base::BindOnce(
          [](LaunchCallback callback,
             borealis::BorealisContextManager::ContextOrFailure
                 context_or_failure) {
            if (!context_or_failure) {
              std::stringstream error_msg;
              error_msg << "Failed to launch ("
                        << static_cast<int>(context_or_failure.Error().error())
                        << "): " << context_or_failure.Error().description();
              std::move(callback).Run(
                  ResponseType::Unexpected(error_msg.str()));
              return;
            }
            std::move(callback).Run(Success(
                context_or_failure.Value()->vm_name(), /*container_name=*/""));
          },
          std::move(callback)));
}

void LaunchCrostini(Profile* profile,
                    bool just_termina,
                    LaunchCallback callback) {
  crostini::CrostiniManager::RestartOptions options;
  options.start_vm_only = just_termina;
  auto container_id = crostini::DefaultContainerId();
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostiniWithOptions(
      container_id, std::move(options),
      base::BindOnce(
          [](std::string vm_name, std::string container_name,
             LaunchCallback callback, crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              std::stringstream error_msg;
              error_msg << "Failed to launch: code="
                        << static_cast<int>(result);
              std::move(callback).Run(
                  ResponseType::Unexpected(error_msg.str()));
              return;
            }
            std::move(callback).Run(Success(vm_name, container_name));
          },
          container_id.vm_name, just_termina ? "" : container_id.container_name,
          std::move(callback)));
}

void LaunchPluginVm(Profile* profile, LaunchCallback callback) {
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile)->LaunchPluginVm(
      base::BindOnce(
          [](LaunchCallback callback, bool success) {
            if (!success) {
              std::move(callback).Run(
                  ResponseType::Unexpected("Failed to launch Plugin VM"));
              return;
            }
            std::move(callback).Run(
                Success(plugin_vm::kPluginVmName, /*container_name=*/""));
          },
          std::move(callback)));
}

void LaunchBruschetta(Profile* profile,
                      const std::string& name,
                      LaunchCallback callback) {
  auto* service = bruschetta::BruschettaService::GetForProfile(profile);
  auto launcher = service->GetLauncher(name);
  if (!launcher) {
    std::move(callback).Run(ResponseType::Unexpected(
        "No record found of a Bruschetta VM named " + name));
    return;
  }
  launcher->EnsureRunning(base::BindOnce(
      [](std::string name, LaunchCallback callback,
         bruschetta::BruschettaResult result) {
        if (result != bruschetta::BruschettaResult::kSuccess) {
          std::move(callback).Run(
              ResponseType::Unexpected("Failed to launch Bruschetta"));
          return;
        }
        std::move(callback).Run(Success(name, /*container_name=*/"penguin"));
      },
      name, std::move(callback)));
}

}  // namespace

void EnsureLaunched(const vm_tools::launch::EnsureVmLaunchedRequest& request,
                    LaunchCallback response_callback) {
  if (request.launch_descriptors().empty()) {
    std::move(response_callback)
        .Run(ResponseType::Unexpected("No launch_descriptors provided"));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.owner_id()) {
    std::move(response_callback)
        .Run(ResponseType::Unexpected(
            "Provided owner_id does not match the primary profile"));
    return;
  }

  // Descriptors are are an increasingly-specific list of identifiers for what
  // the user wants chrome to launch.
  //
  // E.g. ["crostini"] may refer to the default linux container, whereas
  // ["crostini", "foo"] could instead refer to a custom linux container called
  // "foo".
  const std::string& main_descriptor = request.launch_descriptors()[0];
  if (main_descriptor == "borealis") {
    LaunchBorealis(profile, std::move(response_callback));
  } else if (main_descriptor == "crostini") {
    LaunchCrostini(profile, /*just_termina=*/false,
                   std::move(response_callback));
  } else if (main_descriptor == "plugin_vm") {
    LaunchPluginVm(profile, std::move(response_callback));
  } else if (main_descriptor == "termina") {
    LaunchCrostini(profile, /*just_termina=*/true,
                   std::move(response_callback));
  } else if (main_descriptor == "bruschetta") {
    if (request.launch_descriptors().size() == 1) {
      std::move(response_callback)
          .Run(ResponseType::Unexpected(
              "Error: Bruschetta needs a name to launch"));
      return;
    }
    const std::string& name = request.launch_descriptors()[1];
    LaunchBruschetta(profile, name, std::move(response_callback));
  } else {
    std::move(response_callback)
        .Run(
            ResponseType::Unexpected("Unknown descriptor: " + main_descriptor));
  }
}

}  // namespace guest_os::launcher
