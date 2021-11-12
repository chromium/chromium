// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

DeviceTrustKeyManagerImpl::DeviceTrustKeyManagerImpl(
    std::unique_ptr<KeyRotationLauncher> key_rotation_launcher)
    : key_rotation_launcher_(std::move(key_rotation_launcher)) {
  DCHECK(key_rotation_launcher_);
}

DeviceTrustKeyManagerImpl::~DeviceTrustKeyManagerImpl() = default;

void DeviceTrustKeyManagerImpl::StartInitialization() {
  NOTIMPLEMENTED();
}

void DeviceTrustKeyManagerImpl::StartKeyRotation(const std::string& nonce) {
  NOTIMPLEMENTED();
}

void DeviceTrustKeyManagerImpl::ExportPublicKeyAsync(
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  NOTIMPLEMENTED();
}

void DeviceTrustKeyManagerImpl::SignStringAsync(
    const std::string& str,
    base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback) {
  NOTIMPLEMENTED();
}

}  // namespace enterprise_connectors
