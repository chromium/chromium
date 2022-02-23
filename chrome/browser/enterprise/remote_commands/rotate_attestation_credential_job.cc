// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/rotate_attestation_credential_job.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
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

}  // namespace

RotateAttestationCredentialJob::ResultPayload::ResultPayload(
    KeyRotationResult result)
    : result_(result) {
  base::DictionaryValue root_dict;
  root_dict.SetString(kResultFieldName, ResultToString(result_));
  base::JSONWriter::Write(root_dict, &payload_);
}

std::unique_ptr<std::string>
RotateAttestationCredentialJob::ResultPayload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

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
  absl::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;

  if (!root->is_dict())
    return false;

  std::string* nonce_ptr = root->FindStringKey(kNoncePathField);

  if (nonce_ptr && !nonce_ptr->empty()) {
    nonce_ = *nonce_ptr;
    return true;
  }
  return false;
}

void RotateAttestationCredentialJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  DCHECK(nonce_.has_value());

  key_manager_->RotateKey(
      nonce_.value(),
      base::BindOnce(&RotateAttestationCredentialJob::OnKeyRotated,
                     weak_factory_.GetWeakPtr(), std::move(succeeded_callback),
                     std::move(failed_callback)));
}

void RotateAttestationCredentialJob::OnKeyRotated(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    KeyRotationResult rotation_result) {
  auto payload =
      std::make_unique<RotateAttestationCredentialJob::ResultPayload>(
          rotation_result);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(payload->IsSuccess() ? succeeded_callback
                                                    : failed_callback),
                     std::move(payload)));
}

}  // namespace enterprise_commands
