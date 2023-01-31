// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/dlp_files_policy_service_provider.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/dlp/dbus-constants.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Maps dlp::FileAction proto enum to DlpFilesController::FileAction enum.
policy::DlpFilesController::FileAction MapProtoToFileAction(
    dlp::FileAction file_action) {
  switch (file_action) {
    case dlp::FileAction::UPLOAD:
      return policy::DlpFilesController::FileAction::kUpload;
    case dlp::FileAction::COPY:
      return policy::DlpFilesController::FileAction::kCopy;
    case dlp::FileAction::MOVE:
      return policy::DlpFilesController::FileAction::kMove;
    case dlp::FileAction::OPEN:
    // TODO(crbug.com/1378653): Return open FileAction.
    case dlp::FileAction::SHARE:
    // TODO(crbug.com/1378653): Return share FileAction.
    case dlp::FileAction::TRANSFER:
      return policy::DlpFilesController::FileAction::kTransfer;
  }
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
  if (!request.has_source_url()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing source url in request"));
    return;
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  DCHECK(rules_manager);
  policy::DlpFilesController* files_controller =
      rules_manager->GetDlpFilesController();

  // TODO(crbug.com/1360005): Add actual file path.
  bool restricted =
      files_controller
          ? files_controller->IsDlpPolicyMatched(
                policy::DlpFilesController::FileDaemonInfo(
                    request.file_metadata().inode(), base::FilePath(),
                    request.file_metadata().source_url()))
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

  std::vector<policy::DlpFilesController::FileDaemonInfo> files_info;
  for (const auto& file : request.transferred_files()) {
    if (!file.has_inode() || !file.has_path() || !file.has_source_url()) {
      LOG(ERROR) << "Missing file path or file source url";
      continue;
    }
    files_info.emplace_back(file.inode(), base::FilePath(file.path()),
                            file.source_url());
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  DCHECK(rules_manager);
  policy::DlpFilesController* files_controller =
      rules_manager->GetDlpFilesController();
  if (!files_controller) {
    RespondWithRestrictedFilesTransfer(method_call, std::move(response_sender),
                                       std::move(files_info));
    return;
  }

  absl::optional<policy::DlpFilesController::DlpFileDestination> destination;
  if (request.has_destination_component()) {
    destination.emplace(request.destination_component());
  } else {
    destination.emplace(request.destination_url());
  }

  policy::DlpFilesController::FileAction files_action =
      policy::DlpFilesController::FileAction::kTransfer;
  if (request.has_file_action())
    files_action = MapProtoToFileAction(request.file_action());

  files_controller->IsFilesTransferRestricted(
      std::move(files_info), std::move(destination.value()), files_action,
      base::BindOnce(
          &DlpFilesPolicyServiceProvider::RespondWithRestrictedFilesTransfer,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(response_sender)));
}

void DlpFilesPolicyServiceProvider::RespondWithRestrictedFilesTransfer(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::vector<policy::DlpFilesController::FileDaemonInfo>&
        restricted_files) {
  dlp::IsFilesTransferRestrictedResponse response_proto;

  for (const auto& file : restricted_files) {
    dlp::FileMetadata* file_metadata = response_proto.add_restricted_files();
    file_metadata->set_inode(file.inode);
    file_metadata->set_path(file.path.value());
    file_metadata->set_source_url(file.source_url.spec());
  }
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_proto);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
