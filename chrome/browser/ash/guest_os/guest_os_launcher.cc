// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_launcher.h"

#include <sstream>

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
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
  borealis::BorealisServiceFactory::GetForProfile(profile)
      ->ContextManager()
      .StartBorealis(base::BindOnce(
          [](LaunchCallback callback,
             borealis::BorealisContextManager::ContextOrFailure
                 context_or_failure) {
            if (!context_or_failure.has_value()) {
              std::stringstream error_msg;
              error_msg << "Failed to launch ("
                        << static_cast<int>(context_or_failure.error().error())
                        << "): " << context_or_failure.error().description();
              std::move(callback).Run(base::unexpected(error_msg.str()));
              return;
            }
            std::move(callback).Run(Success(
                context_or_failure.value()->vm_name(), /*container_name=*/""));
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
              std::move(callback).Run(base::unexpected(error_msg.str()));
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
                  base::unexpected("Failed to launch Plugin VM"));
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
  auto* service = bruschetta::BruschettaServiceFactory::GetForProfile(profile);
  auto launcher = service->GetLauncher(name);
  if (!launcher) {
    std::move(callback).Run(
        base::unexpected("No record found of a Bruschetta VM named " + name));
    return;
  }
  launcher->EnsureRunning(base::BindOnce(
      [](std::string name, LaunchCallback callback,
         bruschetta::BruschettaResult result) {
        if (result != bruschetta::BruschettaResult::kSuccess) {
          std::move(callback).Run(
              base::unexpected("Failed to launch Bruschetta"));
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
        .Run(base::unexpected("No launch_descriptors provided"));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.owner_id()) {
    std::move(response_callback)
        .Run(base::unexpected(
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
          .Run(base::unexpected("Error: Bruschetta needs a name to launch"));
      return;
    }
    const std::string& name = request.launch_descriptors()[1];
    LaunchBruschetta(profile, name, std::move(response_callback));
  } else {
    std::move(response_callback)
        .Run(base::unexpected("Unknown descriptor: " + main_descriptor));
  }
}

void LaunchApplication(
    Profile* profile,
    const guest_os::GuestId& guest_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<std::string>& files,
    SuccessCallback callback) {
  if (registration.Terminal()) {
    // TODO(crbug.com/41395054): This could be improved by using garcon
    // DesktopFile::GenerateArgvWithFiles().
    std::vector<std::string> terminal_args = {
        registration.ExecutableFileName()};
    terminal_args.insert(terminal_args.end(), files.begin(), files.end());
    guest_os::LaunchTerminal(profile, display_id, guest_id,
                             /*cwd=*/std::string(), terminal_args);
    std::move(callback).Run(true, std::string());
    return;
  }

  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile));
  request.set_vm_name(guest_id.vm_name);
  request.set_container_name(guest_id.container_name);
  request.set_desktop_file_id(registration.DesktopFileId());
  if (registration.IsScaled()) {
    request.set_display_scaling(
        vm_tools::cicerone::LaunchContainerApplicationRequest::SCALED);
  }
  base::ranges::copy(files, google::protobuf::RepeatedFieldBackInserter(
                                request.mutable_files()));

  const std::vector<vm_tools::cicerone::ContainerFeature> container_features =
      crostini::GetContainerFeatures();
  request.mutable_container_features()->Add(container_features.begin(),
                                            container_features.end());

  ash::CiceroneClient::Get()->LaunchContainerApplication(
      std::move(request),
      base::BindOnce(
          [](SuccessCallback callback,
             std::optional<
                 vm_tools::cicerone::LaunchContainerApplicationResponse>
                 response) {
            if (!response) {
              std::move(callback).Run(/*success=*/false,
                                      "Failed to launch application. Empty "
                                      "LaunchContainerApplicationResponse.");
              return;
            }
            std::move(callback).Run(response->success(),
                                    response->failure_reason());
          },
          std::move(callback)));
}

}  // namespace guest_os::launcher
