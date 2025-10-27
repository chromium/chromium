// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/identifiers/profile_id_delegate_impl.h"

#include <utility>

#include "base/check.h"
#include "base/uuid.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#else
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise {

namespace {

const void* const kPresetProfileManagementData = &kPresetProfileManagementData;

// Creates and persists the profile GUID if one does not already exist
void CreateProfileGUID(Profile* profile, const base::FilePath& profile_path) {
  auto* prefs = profile->GetPrefs();
  if (!prefs->GetString(kProfileGUIDPref).empty()) {
    return;
  }

  auto* preset_profile_management_data =
      PresetProfileManagementData::Get(profile);
  std::string profile_guid = preset_profile_management_data->guid();
  if (profile_guid.empty()) {
    profile_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  prefs->SetString(kProfileGUIDPref, std::move(profile_guid));
  preset_profile_management_data->ClearGuid();
}

}  // namespace

PresetProfileManagementData* PresetProfileManagementData::Get(
    Profile* profile) {
  CHECK(profile);

  if (!profile->GetUserData(kPresetProfileManagementData)) {
    profile->SetUserData(
        kPresetProfileManagementData,
        std::make_unique<PresetProfileManagementData>(std::string()));
  }

  return static_cast<PresetProfileManagementData*>(
      profile->GetUserData(kPresetProfileManagementData));
}

void PresetProfileManagementData::SetGuid(std::string guid) {
  CHECK(!guid.empty());
  CHECK(guid_.empty());

  guid_ = std::move(guid);
}

void PresetProfileManagementData::ClearGuid() {
  guid_.clear();
}

PresetProfileManagementData::PresetProfileManagementData(
    std::string preset_guid)
    : guid_(std::move(preset_guid)) {}

PresetProfileManagementData::~PresetProfileManagementData() = default;

ProfileIdDelegateImpl::ProfileIdDelegateImpl(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
  CreateProfileGUID(profile_, profile->GetPath());
}

ProfileIdDelegateImpl::~ProfileIdDelegateImpl() = default;

std::string ProfileIdDelegateImpl::GetDeviceId() {
  return ProfileIdDelegateImpl::GetId();
}

// static
std::string ProfileIdDelegateImpl::GetId() {
#if BUILDFLAG(IS_CHROMEOS)
  // Gets the device ID from cloud policy.
  return policy::GetDeviceName();
#else
  // Gets the device ID from the BrowserDMTokenStorage.
  std::string device_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

#if BUILDFLAG(IS_WIN)
  // On Windows, the combination of the client ID and device serial
  // number are used to form the device ID.
  //
  // Serial number could be empty for various reasons. However, we should still
  // generate a profile ID with whatever we have. Devices without serial number
  // will have higher chance of twin issue but it is still better than no ID at
  // all.
  auto serial_number = base::win::GetSerialNumber();
  if (serial_number) {
    device_id += base::WideToUTF8(*serial_number);
  }
#endif  // BUILDFLAG(IS_WIN)

  return device_id;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace enterprise
