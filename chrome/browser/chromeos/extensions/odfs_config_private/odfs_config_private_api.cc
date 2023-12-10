// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include <string>
#include <vector>

#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"

namespace extensions {

OdfsConfigPrivateGetMountFunction::OdfsConfigPrivateGetMountFunction() =
    default;

OdfsConfigPrivateGetMountFunction::~OdfsConfigPrivateGetMountFunction() =
    default;

ExtensionFunction::ResponseAction OdfsConfigPrivateGetMountFunction::Run() {
  extensions::api::odfs_config_private::MountInfo metadata;
  metadata.mode = chromeos::cloud_storage::GetMicrosoftOneDriveMount(
      Profile::FromBrowserContext(browser_context()));
  return RespondNow(ArgumentList(
      extensions::api::odfs_config_private::GetMount::Results::Create(
          metadata)));
}

OdfsConfigPrivateGetAccountRestrictionsFunction::
    OdfsConfigPrivateGetAccountRestrictionsFunction() = default;

OdfsConfigPrivateGetAccountRestrictionsFunction::
    ~OdfsConfigPrivateGetAccountRestrictionsFunction() = default;

ExtensionFunction::ResponseAction
OdfsConfigPrivateGetAccountRestrictionsFunction::Run() {
  base::Value::List restrictions =
      chromeos::cloud_storage::GetMicrosoftOneDriveAccountRestrictions(
          Profile::FromBrowserContext(browser_context()));
  std::vector<std::string> restrictions_vector;
  for (auto& restriction : restrictions) {
    if (restriction.is_string()) {
      restrictions_vector.emplace_back(std::move(restriction.GetString()));
    }
  }

  extensions::api::odfs_config_private::AccountRestrictionsInfo metadata;
  metadata.restrictions = std::move(restrictions_vector);
  return RespondNow(
      ArgumentList(extensions::api::odfs_config_private::
                       GetAccountRestrictions::Results::Create(metadata)));
}

}  // namespace extensions
