// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_ash.h"

#include "chrome/browser/profiles/profile_manager.h"
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
    : device_key_path_(device_key_path), write_delay_(write_delay) {
  device_key_ =
      std::make_unique<KeyDataProviderFile>(device_key_path_, write_delay_);
  device_key_->AddObserver(this);
}

KeyDataProviderAsh::~KeyDataProviderAsh() {
  device_key_->RemoveObserver(this);
  if (profile_key_) {
    profile_key_->RemoveObserver(this);
  }
}

bool KeyDataProviderAsh::IsReady() {
  DCHECK(device_key_);
  return device_key_->IsReady();
}

std::optional<uint64_t> KeyDataProviderAsh::GetId(
    const std::string& project_name) {
  KeyDataProvider* key_data_provider = GetKeyDataProvider(project_name);
  if (!key_data_provider) {
    return std::nullopt;
  }
  return key_data_provider->GetId(project_name);
}

std::optional<uint64_t> KeyDataProviderAsh::GetSecondaryId(
    const std::string& project_name) {
  const auto* project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);
  if (!project_validator) {
    return std::nullopt;
  }

  // If |project_name| is not of type sequence, return std::nullopt as it
  // should not have a corresponding secondary ID.
  if (project_validator->event_type() != StructuredEventProto::SEQUENCE) {
    return std::nullopt;
  }

  DCHECK(device_key_);
  if (device_key_->IsReady()) {
    return device_key_->GetId(project_name);
  }

  return std::nullopt;
}

KeyData* KeyDataProviderAsh::GetKeyData(const std::string& project_name) {
  auto* key_data_provider = GetKeyDataProvider(project_name);
  if (!key_data_provider || !key_data_provider->IsReady()) {
    return nullptr;
  }

  return key_data_provider->GetKeyData(project_name);
}

void KeyDataProviderAsh::Purge() {
  if (device_key_) {
    device_key_->Purge();
  }

  if (profile_key_) {
    profile_key_->Purge();
  }
}

void KeyDataProviderAsh::OnKeyReady() {
  NotifyKeyReady();
}

void KeyDataProviderAsh::ProfileAdded(const Profile& profile) {
  // Only the primary user's keys should be loaded. If there is already is a
  // profile key, no-op.
  if (profile_key_) {
    return;
  }

  const base::FilePath& profile_path = profile.GetPath();

  profile_key_ = std::make_unique<KeyDataProviderFile>(
      profile_path.Append(kProfileKeyPath), write_delay_);
  profile_key_->AddObserver(this);
}

KeyDataProvider* KeyDataProviderAsh::GetKeyDataProvider(
    const std::string& project_name) {
  const auto* project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);
  if (!project_validator) {
    return nullptr;
  }

  switch (project_validator->id_scope()) {
    case IdScope::kPerProfile: {
      if (profile_key_) {
        return profile_key_.get();
      }
      break;
    }
    case IdScope::kPerDevice: {
      // Retrieve the profile key if the type is a sequence.
      if (project_validator->event_type() == StructuredEventProto::SEQUENCE) {
        return profile_key_ ? profile_key_.get() : nullptr;
      }
      if (device_key_) {
        return device_key_.get();
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return nullptr;
}

}  // namespace metrics::structured
