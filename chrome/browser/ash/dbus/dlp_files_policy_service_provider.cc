// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/dlp_files_policy_service_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/dlp/dbus-constants.h"
#include "url/gurl.h"

namespace ash {

DlpFilesPolicyServiceProvider::DlpFilesPolicyServiceProvider() = default;
DlpFilesPolicyServiceProvider::~DlpFilesPolicyServiceProvider() = default;

void DlpFilesPolicyServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      dlp::kDlpFilesPolicyServiceInterface,
      dlp::kDlpFilesPolicyServiceIsRestrictedMethod,
      base::BindRepeating(&DlpFilesPolicyServiceProvider::IsRestricted,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DlpFilesPolicyServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      dlp::kDlpFilesPolicyServiceInterface,
      dlp::kDlpFilesPolicyServiceIsDlpPolicyMatchedMethod,
      base::BindRepeating(&DlpFilesPolicyServiceProvider::IsDlpPolicyMatched,
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

void DlpFilesPolicyServiceProvider::IsRestricted(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  dlp::IsRestrictedRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse IsRestrictedRequest"));
    return;
  }
  if (request.source_urls_size() == 0) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing source url in request"));
    return;
  }
  if (!request.has_destination_url()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing destination url in request"));
    return;
  }

  bool restricted = false;
  policy::DlpRulesManager* dlp_rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (dlp_rules_manager) {
    for (const auto& source_url : request.source_urls()) {
      policy::DlpRulesManager::Level level =
          dlp_rules_manager->IsRestrictedDestination(
              GURL(source_url), GURL(request.destination_url()),
              policy::DlpRulesManager::Restriction::kFiles,
              /*out_source_pattern=*/nullptr,
              /*out_destination_pattern=*/nullptr);
      if (level == policy::DlpRulesManager::Level::kBlock)
        restricted = true;
    }
  }

  dlp::IsRestrictedResponse response_proto;
  response_proto.set_restricted(restricted);
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_proto);
  std::move(response_sender).Run(std::move(response));
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

  bool restricted = false;
  policy::DlpRulesManager* dlp_rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (dlp_rules_manager) {
    policy::DlpRulesManager::Level level =
        dlp_rules_manager->IsRestrictedByAnyRule(
            GURL(request.source_url()),
            policy::DlpRulesManager::Restriction::kFiles);
    if (level == policy::DlpRulesManager::Level::kBlock)
      restricted = true;
  }

  dlp::IsDlpPolicyMatchedResponse response_proto;
  response_proto.set_restricted(restricted);
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_proto);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
