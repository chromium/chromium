// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_ash.h"

#include "components/metrics/structured/key_data_provider_file.h"
#include "components/metrics/structured/structured_metrics_validator.h"

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

bool KeyDataProviderAsh::IsReady() {
  DCHECK(device_key_);
  return device_key_->IsReady();
}

void KeyDataProviderAsh::OnKeyReady() {
  DCHECK(device_key_);
  return device_key_->OnKeyReady();
}

KeyData* KeyDataProviderAsh::GetKeyData(const std::string& project_name) {
  auto* key_data_provider = GetKeyDataProvider(project_name);
  if (!key_data_provider) {
    return nullptr;
  }
  return key_data_provider->GetKeyData(project_name);
}

absl::optional<uint64_t> KeyDataProviderAsh::GetId(
    const std::string& project_name) {
  KeyDataProvider* key_data_provider = GetKeyDataProvider(project_name);
  if (!key_data_provider) {
    return absl::nullopt;
  }
  return key_data_provider->GetId(project_name);
}

absl::optional<uint64_t> KeyDataProviderAsh::GetSecondaryId(
    const std::string& project_name) {
  auto maybe_project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);
  if (!maybe_project_validator.has_value()) {
    return absl::nullopt;
  }

  // If |project_name| is not of type sequence, return absl::nullopt as it
  // should not have a corresponding secondary ID.
  const auto* project_validator = maybe_project_validator.value();
  if (project_validator->event_type() !=
      StructuredEventProto_EventType_SEQUENCE) {
    return absl::nullopt;
  }

  DCHECK(device_key_);
  if (device_key_->IsReady()) {
    return device_key_->GetId(project_name);
  }

  return absl::nullopt;
}

void KeyDataProviderAsh::InitializeDeviceKey(base::OnceClosure callback) {
  if (HasDeviceKey()) {
    return;
  }

  device_key_ = std::make_unique<KeyDataProviderFile>(
      device_key_path_, write_delay_, std::move(callback));
}

void KeyDataProviderAsh::InitializeProfileKey(
    const base::FilePath& profile_path,
    base::OnceClosure callback) {
  // Only the primary user's keys should be loaded. If there is already is a
  // profile key, no-op.
  if (HasProfileKey()) {
    return;
  }

  profile_key_ = std::make_unique<KeyDataProviderFile>(
      profile_path.Append(kProfileKeyPath), write_delay_, std::move(callback));
}

KeyData* KeyDataProviderAsh::GetDeviceKeyData() {
  DCHECK(HasDeviceKey());
  // Project name does not matter for this implementation.
  return device_key_->GetKeyData(/*project_name=*/"");
}

KeyData* KeyDataProviderAsh::GetProfileKeyData() {
  DCHECK(HasProfileKey());
  // Project name does not matter for this implementation.
  return profile_key_->GetKeyData(/*project_name=*/"");
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
  return device_key_ != nullptr;
}

KeyDataProvider* KeyDataProviderAsh::GetKeyDataProvider(
    const std::string& project_name) {
  auto maybe_project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);
  if (!maybe_project_validator.has_value()) {
    return nullptr;
  }
  const auto* project_validator = maybe_project_validator.value();

  switch (project_validator->id_scope()) {
    case IdScope::kPerProfile: {
      if (profile_key_) {
        return profile_key_.get();
      }
      break;
    }
    case IdScope::kPerDevice: {
      // Retrieve the profile key if the type is a sequence.
      if (project_validator->event_type() ==
          StructuredEventProto_EventType_SEQUENCE) {
        return profile_key_ ? profile_key_.get() : nullptr;
      }
      if (device_key_) {
        return device_key_.get();
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  return nullptr;
}

}  // namespace metrics::structured
