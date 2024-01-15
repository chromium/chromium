// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"

namespace enterprise_connectors {

class KeyRotationLauncher;
class SigningKeyPair;

class DeviceTrustKeyManagerImpl : public DeviceTrustKeyManager {
 public:
  using RotateKeyCallback =
      base::OnceCallback<void(DeviceTrustKeyManager::KeyRotationResult)>;
  using ExportPublicKeyCallback =
      base::OnceCallback<void(std::optional<std::string>)>;
  using SignStringCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;

  DeviceTrustKeyManagerImpl(
      std::unique_ptr<KeyRotationLauncher> key_rotation_launcher,
      std::unique_ptr<KeyLoader> key_loader);
  ~DeviceTrustKeyManagerImpl() override;

  // DeviceTrustKeyManager:
  void StartInitialization() override;
  void RotateKey(const std::string& nonce, RotateKeyCallback callback) override;
  void ExportPublicKeyAsync(ExportPublicKeyCallback callback) override;
  void SignStringAsync(const std::string& str,
                       SignStringCallback callback) override;
  std::optional<KeyMetadata> GetLoadedKeyMetadata() const override;
  bool HasPermanentFailure() const override;

 private:
  enum class InitializationState { kDefault, kLoadingKey, kRotatingKey };

  struct RotateKeyRequest {
    RotateKeyRequest(const std::string& nonce_param,
                     RotateKeyCallback callback_param);
    ~RotateKeyRequest();

    std::string nonce;
    RotateKeyCallback callback;
  };

  // Starts a background task to try to load and synchronize the key. If
  // `create_on_fail` is true, a key-creation task will be started if key
  // loading fails. If it's false, the manager will simply respond to all
  // pending callbacks.
  void LoadKey(bool create_on_fail);
  void OnKeyLoaded(bool create_on_fail, KeyLoader::DTCLoadKeyResult result);

  // Starts a background task to try and rotate the key. Forwards `nonce` to
  // the process in charge of handling the key rotation and upload. An empty
  // nonce can be used to represent a key creation task, which means no key
  // previously existed.
  void StartKeyRotationInner(const std::string& nonce,
                             RotateKeyCallback callback);

  // Invoked when the background key rotation tasks completes with a
  // `result_status`. `nonce` captures the parameter given to that process
  // when it was started.
  void OnKeyRotationFinished(const std::string& nonce,
                             RotateKeyCallback callback,
                             KeyRotationCommand::Status result_status);

  // Adds `pending_request` to the list of pending client requests. Also calls
  // the idempotent initialization process. If the manager is currently is
  // a bad state, it will attempt to fix itself. A bad state example could be
  // that the first initialization's create key process failed - this hook will
  // attempt to retry. If the manager is already in the process of being
  // initialized (or doing a key rotation), this call will effectively be a
  // no-op.
  // `pending_request` must not be bound a strong pointer.
  void AddPendingRequest(base::OnceClosure pending_request);

  // Runs all of the pending callbacks. Will reply with valid values if the
  // manager is fully initialized. Will reply with empty values if not.
  void ResumePendingCallbacks();
  void ResumeExportPublicKey(ExportPublicKeyCallback callback);
  void ResumeSignString(const std::string& str, SignStringCallback callback);

  // Resumes pending key rotation requests currently stored in
  // `pending_rotation_request_`. Returns true if there was a pending request
  // and it was resumed, false if not.
  bool TryResumePendingRotationRequest();

  bool IsFullyInitialized() const;

  // Owned instance in charge of creating and launching key rotation commands.
  std::unique_ptr<KeyRotationLauncher> key_rotation_launcher_;

  // Owned instance in charge of loading and performing key synchronization on
  // the signing key.
  std::unique_ptr<KeyLoader> key_loader_;

  // The manager's current initialization state. Depending on its value,
  // incoming client requests can be marked as pending.
  InitializationState state_ = InitializationState::kDefault;

  // Currently loaded device-trust key pair. If nullptr, it effectively means
  // that a key hasn't been loaded into memory yet.
  scoped_refptr<SigningKeyPair> key_pair_;

  // When set, represents the response code for the synchronization request
  // of `key_pair_`.
  std::optional<int> sync_key_response_code_;

  // If a failure deemed as "permanent" (i.e. no use in retrying) is
  // encountered, the key manager flows will be disabled.
  std::optional<PermanentFailure> permanent_failure_;

  // List of pending client requests.
  base::OnceClosureList pending_client_requests_;

  // Represents whether the last key rotation process was successful or not.
  bool key_rotation_succeeded_{false};

  // Potentially holds a remote key rotation request parameters.
  // Whenever the key manager is done doing what it is currently doing, it will
  // start a key rotation process with them.
  std::unique_ptr<RotateKeyRequest> pending_rotation_request_;

  // Runner for tasks needed to be run in the background.
  // TODO(b/210108864): Add background tasks counter to allow DCHECKing that
  // no tasks are running the background during key rotation.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Checker used to validate that non-background tasks should be
  // running on the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DeviceTrustKeyManagerImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_
