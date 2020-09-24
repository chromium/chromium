// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/guest_os_file_tasks.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/layout.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

const char kGuestOsAppActionID[] = "open-with";

namespace {

// When MIME type detection is done; if we can't be properly determined then
// the detection system will end up guessing one of the 2 values below depending
// upon whether it "thinks" it is binary or text content.
constexpr char kUnknownBinaryMimeType[] = "application/octet-stream";
constexpr char kUnknownTextMimeType[] = "text/plain";
constexpr char kPluginVmAppNameSuffix[] = " (Windows)";

bool HasSupportedMimeType(
    const std::set<std::string>& supported_mime_types,
    const std::string& vm_name,
    const std::string& container_name,
    const crostini::CrostiniMimeTypesService& mime_types_service,
    const extensions::EntryInfo& entry) {
  if (supported_mime_types.find(entry.mime_type) !=
      supported_mime_types.end()) {
    return true;
  }

  // Allow files with type text/* to be opened with a text/plain application
  // as per xdg spec.
  // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html.
  // TODO(crbug.com/1032910): Add xdg mime support for aliases, subclasses.
  if (base::StartsWith(entry.mime_type, "text/",
                       base::CompareCase::SENSITIVE) &&
      supported_mime_types.find(kUnknownTextMimeType) !=
          supported_mime_types.end()) {
    return true;
  }

  // If we see either of these then we use the Linux container MIME type
  // mappings as alternates for finding an appropriate app since these are
  // the defaults when Chrome can't figure out the exact MIME type (but they
  // can also be the actual MIME type, so we don't exclude them above).
  if (entry.mime_type == kUnknownBinaryMimeType ||
      entry.mime_type == kUnknownTextMimeType) {
    const std::string& alternate_mime_type =
        mime_types_service.GetMimeType(entry.path, vm_name, container_name);
    if (supported_mime_types.find(alternate_mime_type) !=
        supported_mime_types.end()) {
      return true;
    }
  }

  return false;
}

bool AppSupportsMimeTypeOfAllEntries(
    const crostini::CrostiniMimeTypesService& mime_types_service,
    const std::vector<extensions::EntryInfo>& entries,
    const guest_os::GuestOsRegistryService::Registration& app) {
  return std::all_of(
      cbegin(entries), cend(entries),
      // Get fields once, as their getters are not cheap.
      [supported_mime_types = app.MimeTypes(), vm_name = app.VmName(),
       container_name = app.ContainerName(),
       &mime_types_service](const auto& entry) {
        return HasSupportedMimeType(supported_mime_types, vm_name,
                                    container_name, mime_types_service, entry);
      });
}

bool HasSupportedExtension(const std::set<std::string>& supported_extensions,
                           const extensions::EntryInfo& entry) {
  const auto& extension = entry.path.Extension();
  if (extension.size() <= 1 || extension[0] != '.')
    return false;
  // Strip the leading period.
  return supported_extensions.find({extension.begin() + 1, extension.end()}) !=
         supported_extensions.end();
}

bool AppSupportsExtensionOfAllEntries(
    const std::vector<extensions::EntryInfo>& entries,
    const guest_os::GuestOsRegistryService::Registration& app) {
  return std::all_of(
      cbegin(entries), cend(entries),
      [supported_extensions = app.Extensions()](const auto& entry) {
        return HasSupportedExtension(supported_extensions, entry);
      });
}

auto ConvertLaunchPluginVmAppResultToTaskResult(
    plugin_vm::LaunchPluginVmAppResult result) {
  // TODO(benwells): return the correct code here, depending on how the app will
  // be opened in multiprofile.
  namespace fmp = extensions::api::file_manager_private;
  switch (result) {
    case plugin_vm::LaunchPluginVmAppResult::SUCCESS:
      return fmp::TASK_RESULT_MESSAGE_SENT;
    case plugin_vm::LaunchPluginVmAppResult::FAILED_DIRECTORY_NOT_SHARED:
      return fmp::TASK_RESULT_FAILED_PLUGIN_VM_TASK_DIRECTORY_NOT_SHARED;
    case plugin_vm::LaunchPluginVmAppResult::FAILED_FILE_ON_EXTERNAL_DRIVE:
      return fmp::TASK_RESULT_FAILED_PLUGIN_VM_TASK_EXTERNAL_DRIVE;
    case plugin_vm::LaunchPluginVmAppResult::FAILED:
      return fmp::TASK_RESULT_FAILED;
  }
}

}  // namespace

void FindGuestOsApps(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    const std::vector<GURL>& file_urls,
    std::vector<std::string>* app_ids,
    std::vector<std::string>* app_names,
    std::vector<guest_os::GuestOsRegistryService::VmType>* vm_types) {
  // Ensure all files can be shared with VMs.
  storage::FileSystemContext* file_system_context =
      util::GetFileSystemContextForExtensionId(profile, kFileManagerAppId);
  base::FilePath dummy_vm_mount("/");
  base::FilePath not_used;
  for (const GURL& file_url : file_urls) {
    if (!file_manager::util::ConvertFileSystemURLToPathInsideVM(
            profile, file_system_context->CrackURL(file_url), dummy_vm_mount,
            &not_used)) {
      return;
    }
  }

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  crostini::CrostiniMimeTypesService* mime_types_service =
      crostini::CrostiniMimeTypesServiceFactory::GetForProfile(profile);
  for (const auto& pair : registry_service->GetEnabledApps()) {
    const std::string& app_id = pair.first;
    const auto& registration = pair.second;

    guest_os::GuestOsRegistryService::VmType vm_type = registration.VmType();
    switch (vm_type) {
      case guest_os::GuestOsRegistryService::VmType::
          ApplicationList_VmType_TERMINA:
        if (!AppSupportsMimeTypeOfAllEntries(*mime_types_service, entries,
                                             registration)) {
          continue;
        }
        app_names->push_back(registration.Name());
        break;

      case guest_os::GuestOsRegistryService::VmType::
          ApplicationList_VmType_PLUGIN_VM:
        if (!AppSupportsExtensionOfAllEntries(entries, registration)) {
          continue;
        }
        app_names->push_back(registration.Name() + kPluginVmAppNameSuffix);
        break;

      default:
        LOG(ERROR) << "Unsupported VmType: " << static_cast<int>(vm_type);
        continue;
    }

    app_ids->push_back(app_id);
    vm_types->push_back(registration.VmType());
  }
}

void FindGuestOsTasks(Profile* profile,
                      const std::vector<extensions::EntryInfo>& entries,
                      const std::vector<GURL>& file_urls,
                      std::vector<FullTaskDescriptor>* result_list,
                      base::OnceClosure completion_closure) {
  bool crostini_enabled = crostini::CrostiniFeatures::Get()->IsEnabled(profile);
  bool plugin_vm_enabled =
      plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile);

  if (!crostini_enabled && !plugin_vm_enabled) {
    std::move(completion_closure).Run();
    return;
  }

  std::vector<std::string> result_app_ids;
  std::vector<std::string> result_app_names;
  std::vector<guest_os::GuestOsRegistryService::VmType> result_vm_types;
  FindGuestOsApps(profile, entries, file_urls, &result_app_ids,
                  &result_app_names, &result_vm_types);

  if (result_app_ids.empty()) {
    std::move(completion_closure).Run();
    return;
  }

  std::vector<TaskType> task_types;
  task_types.reserve(result_vm_types.size());
  for (auto vm_type : result_vm_types) {
    switch (vm_type) {
      case guest_os::GuestOsRegistryService::VmType::
          ApplicationList_VmType_TERMINA:
        task_types.push_back(TASK_TYPE_CROSTINI_APP);
        break;
      case guest_os::GuestOsRegistryService::VmType::
          ApplicationList_VmType_PLUGIN_VM:
        task_types.push_back(TASK_TYPE_PLUGIN_VM_APP);
        break;
      default:
        LOG(ERROR) << "Unsupported VmType: " << static_cast<int>(vm_type);
        return;
    }
  }

  for (size_t i = 0; i < result_app_ids.size(); ++i) {
    GURL icon_url(
        base::StrCat({chrome::kChromeUIAppIconURL, result_app_ids[i], "/32"}));
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(result_app_ids[i], task_types[i], kGuestOsAppActionID),
        result_app_names[i],
        extensions::api::file_manager_private::Verb::VERB_OPEN_WITH, icon_url,
        /*is_default=*/false, /*is_generic=*/false,
        /*is_file_extension_match=*/false));
  }

  std::move(completion_closure).Run();
}

void ExecuteGuestOsTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    FileTaskFinishedCallback done) {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);

  base::Optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(task.app_id);
  if (!registration) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Unknown app_id: " + task.app_id);
    return;
  }

  using LaunchArg = absl::variant<storage::FileSystemURL, std::string>;
  std::vector<LaunchArg> args;
  args.reserve(file_system_urls.size());
  for (const auto& url : file_system_urls) {
    args.emplace_back(url);
  }
  guest_os::GuestOsRegistryService::VmType vm_type = registration->VmType();
  switch (vm_type) {
    case guest_os::GuestOsRegistryService::VmType::
        ApplicationList_VmType_TERMINA:
      DCHECK(crostini::CrostiniFeatures::Get()->IsUIAllowed(profile));
      crostini::LaunchCrostiniApp(
          profile, task.app_id, display::kInvalidDisplayId, args,
          base::BindOnce(
              [](FileTaskFinishedCallback done, bool success,
                 const std::string& failure_reason) {
                if (!success) {
                  LOG(ERROR) << "Crostini task error: " << failure_reason;
                }
                std::move(done).Run(
                    // TODO(benwells): return the correct code here, depending
                    // on how the app will be opened in multiprofile.
                    success ? extensions::api::file_manager_private::
                                  TASK_RESULT_MESSAGE_SENT
                            : extensions::api::file_manager_private::
                                  TASK_RESULT_FAILED,
                    failure_reason);
              },
              std::move(done)));
      return;
    case guest_os::GuestOsRegistryService::VmType::
        ApplicationList_VmType_PLUGIN_VM:
      DCHECK(plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile));
      plugin_vm::LaunchPluginVmApp(
          profile, task.app_id, args,
          base::BindOnce(
              [](FileTaskFinishedCallback done,
                 plugin_vm::LaunchPluginVmAppResult result,
                 const std::string& failure_reason) {
                if (result != plugin_vm::LaunchPluginVmAppResult::SUCCESS) {
                  LOG(ERROR) << "Plugin VM task error: " << failure_reason;
                }
                std::move(done).Run(
                    ConvertLaunchPluginVmAppResultToTaskResult(result),
                    failure_reason);
              },
              std::move(done)));
      return;
    default:
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_FAILED,
          base::StringPrintf("Unsupported VmType: %d",
                             static_cast<int>(vm_type)));
  }
}

}  // namespace file_tasks
}  // namespace file_manager
