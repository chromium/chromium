// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_ash.h"

namespace metrics::structured {
namespace {

// Default delay period for the PersistentProto. This is the delay before a file
// write is triggered after a change has been made.
constexpr base::TimeDelta kSaveDelay = base::Milliseconds(1000);

// The path used to store per-profile keys. Relative to the user's
// cryptohome. This file is created by chromium.
constexpr char kProfileKeyPath[] = "structured_metrics/keys";

// The path used to store per-device keys. This file is created by tmpfiles.d
// on start and has its permissions and ownership set such that it is writable
// by chronos.
constexpr char kDeviceKeyPath[] = "/var/lib/metrics/structured/chromium/keys";

}  // namespace

KeyDataProviderAsh::KeyDataProviderAsh()
    : KeyDataProviderAsh(base::FilePath(kDeviceKeyPath), kSaveDelay) {}

KeyDataProviderAsh::KeyDataProviderAsh(const base::FilePath& device_key_path,
                                       base::TimeDelta write_delay)
    : device_key_path_(device_key_path), write_delay_(write_delay) {}

KeyDataProviderAsh::~KeyDataProviderAsh() = default;

void KeyDataProviderAsh::InitializeDeviceKey(base::OnceClosure callback) {
  if (HasDeviceKey()) {
    return;
  }

  device_key_ = std::make_unique<KeyData>(device_key_path_, write_delay_,
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
                                           write_delay_, std::move(callback));
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
