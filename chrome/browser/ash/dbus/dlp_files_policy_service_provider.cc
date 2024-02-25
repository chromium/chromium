// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/dlp_files_policy_service_provider.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/dlp/dbus-constants.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Maps dlp::FileAction proto enum to DlpFilesController::FileAction enum.
policy::dlp::FileAction MapProtoToFileAction(dlp::FileAction file_action) {
  switch (file_action) {
    case dlp::FileAction::UPLOAD:
      return policy::dlp::FileAction::kUpload;
    case dlp::FileAction::COPY:
      return policy::dlp::FileAction::kCopy;
    case dlp::FileAction::MOVE:
      return policy::dlp::FileAction::kMove;
    case dlp::FileAction::OPEN:
      return policy::dlp::FileAction::kOpen;
    case dlp::FileAction::SHARE:
      return policy::dlp::FileAction::kShare;
    case dlp::FileAction::TRANSFER:
      return policy::dlp::FileAction::kTransfer;
  }
}

// Maps |component| to data_controls::Component.
data_controls::Component MapProtoToPolicyComponent(
    ::dlp::DlpComponent component) {
  switch (component) {
    case ::dlp::DlpComponent::ARC:
      return data_controls::Component::kArc;
    case ::dlp::DlpComponent::CROSTINI:
      return data_controls::Component::kCrostini;
    case ::dlp::DlpComponent::PLUGIN_VM:
      return data_controls::Component::kPluginVm;
    case ::dlp::DlpComponent::USB:
      return data_controls::Component::kUsb;
    case ::dlp::DlpComponent::GOOGLE_DRIVE:
      return data_controls::Component::kDrive;
    case ::dlp::DlpComponent::MICROSOFT_ONEDRIVE:
      return data_controls::Component::kOneDrive;
    case ::dlp::DlpComponent::UNKNOWN_COMPONENT:
    case ::dlp::DlpComponent::SYSTEM:
      return data_controls::Component::kUnknownComponent;
  }
}

// Called when restricted files sources are obtained.
void RespondWithRestrictedFilesTransfer(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::vector<std::pair<policy::DlpFilesControllerAsh::FileDaemonInfo,
                                dlp::RestrictionLevel>>& requested_files) {
  dlp::IsFilesTransferRestrictedResponse response_proto;

  for (const auto& [file, level] : requested_files) {
    dlp::FileRestriction* files_restriction =
        response_proto.add_files_restrictions();
    files_restriction->mutable_file_metadata()->set_inode(file.inode);
    files_restriction->mutable_file_metadata()->set_crtime(file.crtime);
    files_restriction->mutable_file_metadata()->set_path(file.path.value());
    files_restriction->mutable_file_metadata()->set_source_url(
        file.source_url.spec());
    files_restriction->mutable_file_metadata()->set_referrer_url(
        file.referrer_url.spec());
    files_restriction->set_restriction_level(level);
  }
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_proto);
  std::move(response_sender).Run(std::move(response));
}

// Respond with a single restriction level.
void DirectRespondWithRestrictedFilesTransfer(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::vector<policy::DlpFilesControllerAsh::FileDaemonInfo>&
        files_info,
    ::dlp::RestrictionLevel restriction_level) {
  std::vector<std::pair<policy::DlpFilesControllerAsh::FileDaemonInfo,
                        dlp::RestrictionLevel>>
      response_files;
  for (const auto& file : files_info) {
    response_files.emplace_back(file, restriction_level);
  }
  RespondWithRestrictedFilesTransfer(method_call, std::move(response_sender),
                                     std::move(response_files));
}

}  // namespace

DlpFilesPolicyServiceProvider::DlpFilesPolicyServiceProvider() = default;
DlpFilesPolicyServiceProvider::~DlpFilesPolicyServiceProvider() = default;

void DlpFilesPolicyServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      dlp::kDlpFilesPolicyServiceInterface,
      dlp::kDlpFilesPolicyServiceIsDlpPolicyMatchedMethod,
      base::BindRepeating(&DlpFilesPolicyServiceProvider::IsDlpPolicyMatched,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DlpFilesPolicyServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      dlp::kDlpFilesPolicyServiceInterface,
      dlp::kDlpFilesPolicyServiceIsFilesTransferRestrictedMethod,
      base::BindRepeating(
          &DlpFilesPolicyServiceProvider::IsFilesTransferRestricted,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DlpFilesPolicyServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DlpFilesPolicyServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void DlpFilesPolicyServiceProvider::IsDlpPolicyMatched(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  dlp::IsDlpPolicyMatchedRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse IsDlpPolicyMatchedRequest"));
    return;
  }

  policy::DlpFilesControllerAsh* files_controller =
      policy::DlpFilesControllerAsh::GetForPrimaryProfile();

  bool restricted =
      files_controller
          ? files_controller->IsDlpPolicyMatched(
                policy::DlpFilesControllerAsh::FileDaemonInfo(
                    request.file_metadata().inode(),
                    request.file_metadata().crtime(), base::FilePath(),
                    request.file_metadata().source_url(),
                    request.file_metadata().referrer_url()))
          : false;

  dlp::IsDlpPolicyMatchedResponse response_proto;
  response_proto.set_restricted(restricted);
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_proto);
  std::move(response_sender).Run(std::move(response));
}

void DlpFilesPolicyServiceProvider::IsFilesTransferRestricted(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  dlp::IsFilesTransferRestrictedRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse IsFilesTransferRestrictedRequest"));
    return;
  }
  if (!request.has_destination_url() && !request.has_destination_component()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing both destination url and component in request"));
    return;
  }

  std::vector<policy::DlpFilesControllerAsh::FileDaemonInfo> files_info;
  for (const auto& file : request.transferred_files()) {
    if (!file.has_inode() || !file.has_path() || !file.has_source_url()) {
      LOG(ERROR) << "Missing file path or file source url";
      continue;
    }
    files_info.emplace_back(file.inode(), file.crtime(),
                            base::FilePath(file.path()), file.source_url(),
                            file.referrer_url());
  }

  // Transfer to local file system or access by system components is always
  // allowed.
  if (request.has_destination_component() &&
      request.destination_component() == dlp::SYSTEM) {
    DirectRespondWithRestrictedFilesTransfer(
        method_call, std::move(response_sender), files_info,
        ::dlp::RestrictionLevel::LEVEL_ALLOW);
    return;
  }

  policy::DlpFilesControllerAsh* files_controller =
      policy::DlpFilesControllerAsh::GetForPrimaryProfile();
  if (!files_controller) {
    DirectRespondWithRestrictedFilesTransfer(
        method_call, std::move(response_sender), files_info,
        ::dlp::RestrictionLevel::LEVEL_UNSPECIFIED);
    return;
  }

  std::optional<policy::DlpFileDestination> destination;
  if (request.has_destination_component()) {
    destination.emplace(
        MapProtoToPolicyComponent(request.destination_component()));
  } else {
    destination.emplace(GURL(request.destination_url()));
  }

  policy::dlp::FileAction files_action = policy::dlp::FileAction::kTransfer;
  if (request.has_file_action()) {
    files_action = MapProtoToFileAction(request.file_action());
  }

  std::optional<file_manager::io_task::IOTaskId> task_id = std::nullopt;
  if (request.has_io_task_id()) {
    task_id = request.io_task_id();
  }

  files_controller->IsFilesTransferRestricted(
      std::move(task_id), std::move(files_info), std::move(destination.value()),
      files_action,
      base::BindOnce(&RespondWithRestrictedFilesTransfer, method_call,
                     std::move(response_sender)));
}

}  // namespace ash
