// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_ash.h"

namespace metrics::structured {
namespace {
// The delay period for the PersistentProto.
constexpr int kSaveDelayMs = 1000;

// The path used to store per-profile keys. Relative to the user's
// cryptohome. This file is created by chromium.
constexpr char kProfileKeyPath[] = "structured_metrics/keys";

// The path used to store per-device keys. This file is created by tmpfiles.d
// on start and has its permissions and ownership set such that it is writable
// by chronos.
constexpr char kDeviceKeyPath[] = "/var/lib/metrics/structured/chromium/keys";

}  // namespace

KeyDataProviderAsh::KeyDataProviderAsh()
    : KeyDataProviderAsh(base::FilePath(kDeviceKeyPath), kSaveDelayMs) {}

KeyDataProviderAsh::KeyDataProviderAsh(const base::FilePath& device_key_path,
                                       int write_delay_ms)
    : device_key_path_(device_key_path), write_delay_ms_(write_delay_ms) {}

KeyDataProviderAsh::~KeyDataProviderAsh() = default;

void KeyDataProviderAsh::InitializeDeviceKey(base::OnceClosure callback) {
  if (HasDeviceKey()) {
    return;
  }

  device_key_ = std::make_unique<KeyData>(device_key_path_,
                                          base::Milliseconds(write_delay_ms_),
                                          std::move(callback));
}

void KeyDataProviderAsh::InitializeProfileKey(
    const base::FilePath& profile_path,
    base::OnceClosure callback) {
  // Only the primary user's keys should be loaded. If there is already is a
  // profile key, no-op.
  if (HasProfileKey()) {
    return;
  }

  profile_key_ = std::make_unique<KeyData>(profile_path.Append(kProfileKeyPath),
                                           base::Milliseconds(write_delay_ms_),
                                           std::move(callback));
}

KeyData* KeyDataProviderAsh::GetDeviceKeyData() {
  DCHECK(HasDeviceKey());
  return device_key_.get();
}

KeyData* KeyDataProviderAsh::GetProfileKeyData() {
  DCHECK(HasProfileKey());
  return profile_key_.get();
}

void KeyDataProviderAsh::Purge() {
  if (HasDeviceKey()) {
    GetDeviceKeyData()->Purge();
  }

  if (HasProfileKey()) {
    GetProfileKeyData()->Purge();
  }
}

bool KeyDataProviderAsh::HasProfileKey() {
  return profile_key_ != nullptr;
}

bool KeyDataProviderAsh::HasDeviceKey() {
  return profile_key_ != nullptr;
}

}  // namespace metrics::structured
