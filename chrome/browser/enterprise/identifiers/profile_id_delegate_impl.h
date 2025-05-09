// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/enterprise/browser/identifiers/profile_id_delegate.h"

class Profile;

namespace enterprise {

// This class manages the collection of data needed for profile management,
// before the new profile is fully initialized. For now this class only contains
// the preset profile GUID for a newly created profile.
class PresetProfileManagementData : public base::SupportsUserData::Data {
 public:
  explicit PresetProfileManagementData(std::string preset_guid);
  PresetProfileManagementData(const PresetProfileManagementData&) = delete;
  PresetProfileManagementData& operator=(const PresetProfileManagementData&) =
      delete;
  ~PresetProfileManagementData() override;

  static PresetProfileManagementData* Get(Profile* profile);
  void SetGuid(std::string guid);
  void ClearGuid();

  // The preset GUID will be used instead of a new random GUID when a profile is
  // first created. This does not overwrite if a GUID has already been set for a
  // profile.
  const std::string& guid() const { return guid_; }

 private:
  std::string guid_;
};

// Implementation of the profile Id delegate.
class ProfileIdDelegateImpl : public ProfileIdDelegate {
 public:
  explicit ProfileIdDelegateImpl(Profile* profile);
  ProfileIdDelegateImpl(const ProfileIdDelegateImpl&) = delete;
  ProfileIdDelegateImpl& operator=(const ProfileIdDelegateImpl&) = delete;
  ~ProfileIdDelegateImpl() override;

  // ProfileIdDelegate
  std::string GetDeviceId() override;

  static std::string GetId();

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise

#endif  // CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IMPL_H_
