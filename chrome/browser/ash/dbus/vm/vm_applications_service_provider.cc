// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_applications_service_provider.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/display/types/display_constants.h"

namespace ash {
namespace {

struct SelectFileData {
  scoped_refptr<SelectFileDialogExtension> dialog;
  vm_tools::cicerone::FileSelectedSignal signal;
};

}  // namespace

VmApplicationsServiceProvider::VmApplicationsServiceProvider() = default;

VmApplicationsServiceProvider::~VmApplicationsServiceProvider() = default;

void VmApplicationsServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceUpdateApplicationListMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::UpdateApplicationList,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceLaunchTerminalMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::LaunchTerminal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceUpdateMimeTypesMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::UpdateMimeTypes,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceSelectFileMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::SelectFile,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmApplicationsServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void VmApplicationsServiceProvider::UpdateApplicationList(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::ApplicationList request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse ApplicationList from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  if (!registry_service) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Shutting down"));
    return;
  }
  registry_service->UpdateApplicationList(request);

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void VmApplicationsServiceProvider::LaunchTerminal(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::TerminalParams request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse TerminalParams from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile) &&
      request.owner_id() == crostini::CryptohomeIdForProfile(profile)) {
    // kInvalidDisplayId will launch terminal on the current active display.
    guest_os::LaunchTerminal(
        profile, display::kInvalidDisplayId,
        guest_os::GuestId(crostini::kCrostiniDefaultVmType, request.vm_name(),
                          request.container_name()),
        request.cwd(),
        std::vector<std::string>(request.params().begin(),
                                 request.params().end()));
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void VmApplicationsServiceProvider::UpdateMimeTypes(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::MimeTypes request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] = "Unable to parse MimeTypes from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* mime_types_service =
      guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile);
  if (!mime_types_service) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Shutting down"));
    return;
  }
  mime_types_service->UpdateMimeTypes(request);

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void VmApplicationsServiceProvider::SelectFile(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::SelectFileRequest request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse SelectFileRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));

  // SelectFileDialog will take ownership of |data| when we call SelectFile(),
  // and we will take back ownership in MultiFilesSelected().
  auto data = std::make_unique<SelectFileData>();
  data->signal.set_vm_name(request.vm_name());
  data->signal.set_container_name(request.container_name());
  data->signal.set_owner_id(request.owner_id());
  data->signal.set_select_file_token(request.select_file_token());

  // Match strings used by FilesApp GetDialogTypeAsString().
  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  if (request.type() == "open-multi-file") {
    type = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
  } else if (request.type() == "saveas-file") {
    type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
  } else if (request.type() == "folder") {
    type = ui::SelectFileDialog::SELECT_FOLDER;
  } else if (request.type() == "upload-folder") {
    type = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
  }
  std::u16string title = base::UTF8ToUTF16(request.title());
  base::FilePath default_path;
  SelectFileDialogExtension::Owner owner;
  if (!request.default_path().empty()) {
    // Parse as file: URL if possible.
    std::vector<ui::FileInfo> file_infos =
        ui::URIListToFileInfos(request.default_path());
    if (file_infos.empty()) {
      file_infos.push_back(ui::FileInfo(base::FilePath(request.default_path()),
                                        base::FilePath()));
    }
    // Translate to path in host and DLP component type if possible.
    ui::EndpointType source = ui::EndpointType::kUnknownVm;
    if (request.vm_name() == crostini::kCrostiniDefaultVmName) {
      source = ui::EndpointType::kCrostini;
      owner.dialog_caller = policy::DlpFileDestination(
          policy::DlpRulesManager::Component::kCrostini);
    }
    std::vector<base::FilePath> paths =
        TranslateVMPathsToHost(source, file_infos);
    default_path =
        !paths.empty() ? std::move(paths[0]) : std::move(file_infos[0].path);
  }
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  int file_type_index = 0;
  ParseSelectFileDialogFileTypes(request.allowed_extensions(), &file_types,
                                 &file_type_index);
  data->dialog = SelectFileDialogExtension::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(nullptr));
  // Release ownership of |data| and take back in MultiFilesSelected().
  void* params = static_cast<void*>(data.get());

  data.release()->dialog->SelectFileWithFileManagerParams(
      type, title, default_path, &file_types, file_type_index, params, owner,
      /*search_query=*/"", /*show_android_picker_apps=*/false);
}

void VmApplicationsServiceProvider::ParseSelectFileDialogFileTypes(
    const std::string& allowed_extensions,
    ui::SelectFileDialog::FileTypeInfo* file_types,
    int* file_type_index) const {
  file_types->extensions.clear();
  file_types->extension_description_overrides.clear();
  file_types->include_all_files = false;
  *file_type_index = 0;

  // First split on '|'.
  auto items = base::SplitString(allowed_extensions, "|", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  int i = 1;
  for (const auto& item : items) {
    // item '*' means add option on dialog to include all files.
    if (item == "*") {
      file_types->include_all_files = true;
      continue;
    }
    std::string extensions = item;
    // Description after ':'.
    std::string desc;
    size_t pos = item.find(':');
    if (pos != std::string::npos) {
      extensions = item.substr(0, pos);
      desc = item.substr(pos + 1);
    }
    // Comma separated list of extensions.
    auto exts = base::SplitString(extensions, ",", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
    if (!exts.empty()) {
      file_types->extensions.push_back(exts);
      file_types->extension_description_overrides.push_back(
          base::UTF8ToUTF16(desc));
      // Leading comma indicates selected item.
      if (item[0] == ',')
        *file_type_index = i;
      ++i;
    }
  }
}

void VmApplicationsServiceProvider::FileSelected(const base::FilePath& path,
                                                 int index,
                                                 void* params) {
  MultiFilesSelected({path}, params);
}

void VmApplicationsServiceProvider::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  auto data =
      base::WrapUnique<SelectFileData>(static_cast<SelectFileData*>(params));

  ui::EndpointType target = ui::EndpointType::kDefault;
  if (data->signal.vm_name() == crostini::kCrostiniDefaultVmName) {
    target = ui::EndpointType::kCrostini;
  }

  ShareWithVMAndTranslateToFileUrls(
      target, files,
      base::BindOnce(
          [](std::unique_ptr<SelectFileData> data,
             std::vector<std::string> file_urls) {
            for (const auto& file_url : file_urls) {
              data->signal.add_files(file_url);
            }
            CiceroneClient::Get()->FileSelected(data->signal);
          },
          std::move(data)));
}

void VmApplicationsServiceProvider::FileSelectionCanceled(void* params) {
  MultiFilesSelected({}, params);
}

}  // namespace ash
