// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_applications_service_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/exo/chrome_security_delegate.h"
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
#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/display/types/display_constants.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace ash {
namespace {

class DialogListener : public ui::SelectFileDialog::Listener {
 public:
  explicit DialogListener(vm_tools::cicerone::FileSelectedSignal signal)
      : dialog_(SelectFileDialogExtension::Create(
            this,
            std::make_unique<ChromeSelectFilePolicy>(nullptr))),
        signal_(signal) {
    CHECK(dialog_);
  }
  DialogListener(const DialogListener&) = delete;
  DialogListener& operator=(const DialogListener&) = delete;
  ~DialogListener() override { dialog_->ListenerDestroyed(); }

  scoped_refptr<SelectFileDialogExtension> dialog() { return dialog_; }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    MultiFilesSelected({file});
  }
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override { MultiFilesSelected({}); }

 private:
  const scoped_refptr<SelectFileDialogExtension> dialog_;
  const vm_tools::cicerone::FileSelectedSignal signal_;
};

void DialogListener::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  ShareWithVMAndTranslateToFileUrls(
      signal_.vm_name(), ui::SelectedFileInfoListToFilePathList(files),
      base::BindOnce(
          [](vm_tools::cicerone::FileSelectedSignal signal,
             std::vector<std::string> file_urls) {
            for (const auto& file_url : file_urls) {
              signal.add_files(file_url);
            }
            CiceroneClient::Get()->FileSelected(signal);
          },
          signal_));
  delete this;
}

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
    std::vector<base::FilePath> paths =
        TranslateVMPathsToHost(request.vm_name(), file_infos);
    default_path =
        !paths.empty() ? std::move(paths[0]) : std::move(file_infos[0].path);
  }
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  int file_type_index = 0;
  ParseSelectFileDialogFileTypes(request.allowed_extensions(), &file_types,
                                 &file_type_index);

  vm_tools::cicerone::FileSelectedSignal signal;
  signal.set_vm_name(request.vm_name());
  signal.set_container_name(request.container_name());
  signal.set_owner_id(request.owner_id());
  signal.set_select_file_token(request.select_file_token());
  auto listener = std::make_unique<DialogListener>(signal);

  // Grab the dialog from `listener` before releasing it.
  scoped_refptr<SelectFileDialogExtension> dialog = listener->dialog();
  // Release ownership of `listener`; it will self-delete when it receives a
  // SelectFile callback.
  listener.release();
  dialog->SelectFileWithFileManagerParams(
      type, title, default_path, &file_types, file_type_index, owner,
      /*search_query=*/"", /*show_android_picker_apps=*/false);
}

// static
void VmApplicationsServiceProvider::ParseSelectFileDialogFileTypes(
    const std::string& allowed_extensions,
    ui::SelectFileDialog::FileTypeInfo* file_types,
    int* file_type_index) {
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

}  // namespace ash
