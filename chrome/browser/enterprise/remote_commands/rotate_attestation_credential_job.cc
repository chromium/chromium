// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/rotate_attestation_credential_job.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"

namespace enterprise_commands {

using DeviceTrustKeyManager = enterprise_connectors::DeviceTrustKeyManager;
using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;

namespace {

const char kNoncePathField[] = "nonce";
const char kResultFieldName[] = "result";

std::string ResultToString(KeyRotationResult result) {
  switch (result) {
    case KeyRotationResult::SUCCESS:
      return "success";
    case KeyRotationResult::FAILURE:
      return "failure";
    case KeyRotationResult::CANCELLATION:
      return "cancellation";
  }
}

std::string CreatePayload(KeyRotationResult result) {
  base::Value::Dict root_dict;
  root_dict.Set(kResultFieldName, ResultToString(result));

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

std::string CreateUnsupportedPayload() {
  base::Value::Dict root_dict;
  root_dict.Set(kResultFieldName, "unsupported");

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

bool IsSuccess(KeyRotationResult result) {
  return result == enterprise_connectors::DeviceTrustKeyManager::
                       KeyRotationResult::SUCCESS;
}

}  // namespace

RotateAttestationCredentialJob::RotateAttestationCredentialJob(
    DeviceTrustKeyManager* key_manager)
    : key_manager_(key_manager) {
  DCHECK(key_manager_);
}

RotateAttestationCredentialJob::~RotateAttestationCredentialJob() = default;

enterprise_management::RemoteCommand_Type
RotateAttestationCredentialJob::GetType() const {
  return enterprise_management::
      RemoteCommand_Type_BROWSER_ROTATE_ATTESTATION_CREDENTIAL;
}

bool RotateAttestationCredentialJob::ParseCommandPayload(
    const std::string& command_payload) {
  std::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;

  if (!root->is_dict())
    return false;

  std::string* nonce_ptr = root->GetDict().FindString(kNoncePathField);

  if (nonce_ptr && !nonce_ptr->empty()) {
    nonce_ = *nonce_ptr;
    return true;
  }
  return false;
}

void RotateAttestationCredentialJob::RunImpl(
    CallbackWithResult result_callback) {
  if (!enterprise_connectors::IsKeyRotationEnabled()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), policy::ResultType::kFailure,
                       CreateUnsupportedPayload()));
    return;
  }

  DCHECK(nonce_.has_value());

  key_manager_->RotateKey(
      nonce_.value(),
      base::BindOnce(&RotateAttestationCredentialJob::OnKeyRotated,
                     weak_factory_.GetWeakPtr(), std::move(result_callback)));
}

void RotateAttestationCredentialJob::OnKeyRotated(
    CallbackWithResult result_callback,
    KeyRotationResult rotation_result) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback),
                                std::move(IsSuccess(rotation_result))
                                    ? policy::ResultType::kSuccess
                                    : policy::ResultType::kFailure,
                                CreatePayload(rotation_result)));
}

}  // namespace enterprise_commands
