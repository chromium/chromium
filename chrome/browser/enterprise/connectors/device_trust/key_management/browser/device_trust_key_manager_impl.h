// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"

namespace enterprise_connectors {

class KeyRotationLauncher;
class SigningKeyPair;

class DeviceTrustKeyManagerImpl : public DeviceTrustKeyManager {
 public:
  using ExportPublicKeyCallback =
      base::OnceCallback<void(absl::optional<std::string>)>;
  using SignStringCallback =
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)>;

  explicit DeviceTrustKeyManagerImpl(
      std::unique_ptr<KeyRotationLauncher> key_rotation_launcher);
  ~DeviceTrustKeyManagerImpl() override;

  // DeviceTrustKeyManager:
  void StartInitialization() override;
  void StartKeyRotation(const std::string& nonce) override;
  void ExportPublicKeyAsync(ExportPublicKeyCallback callback) override;
  void SignStringAsync(const std::string& str,
                       SignStringCallback callback) override;

  bool is_fully_initialized() {
    return state_ == InitializationState::kDefault && key_pair_;
  }

 private:
  enum class InitializationState {
    kDefault,
    kLoadingKey,
    kStartingKeyRotation,
    kWaitingForKeyRotation
  };

  void LoadKey();
  void OnKeyLoaded(std::unique_ptr<SigningKeyPair> loaded_key_pair);

  void StartKeyRotationInner(const std::string& nonce);
  void OnKeyRotationStarted(bool rotation_started);

  std::unique_ptr<KeyRotationLauncher> key_rotation_launcher_;

  InitializationState state_ = InitializationState::kDefault;
  std::unique_ptr<SigningKeyPair> key_pair_;

  // Runner for tasks needed to be run in the background.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Checker used to validate that non-background tasks should be
  // running on the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DeviceTrustKeyManagerImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_
