// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

base::Value::List ToList(const std::vector<std::string>& values) {
  base::Value::List list;
  for (const auto& value : values) {
    list.Append(value);
  }
  return list;
}

}  // namespace

class OfdsConfigPrivateApiUnittest : public ExtensionApiUnittest {
 public:
  OfdsConfigPrivateApiUnittest() = default;
  OfdsConfigPrivateApiUnittest(const OfdsConfigPrivateApiUnittest&) = delete;
  OfdsConfigPrivateApiUnittest& operator=(const OfdsConfigPrivateApiUnittest&) =
      delete;
  ~OfdsConfigPrivateApiUnittest() override = default;

 protected:
  void SetOneDriveMount(Profile* profile, const std::string& mount) {
    ASSERT_TRUE(profile);
    profile->GetPrefs()->SetString(prefs::kMicrosoftOneDriveMount, mount);
  }

  void SetOneDriveAccountRestrictions(
      Profile* profile,
      const std::vector<std::string>& restrictions) {
    ASSERT_TRUE(profile);
    profile->GetPrefs()->SetList(prefs::kMicrosoftOneDriveAccountRestrictions,
                                 ToList(restrictions));
  }
};

TEST_F(OfdsConfigPrivateApiUnittest, GetMountSuccessful) {
  struct {
    std::string policy_value;
    extensions::api::odfs_config_private::Mount expected_mode;
  } test_cases[] = {
      {"allowed", extensions::api::odfs_config_private::Mount::kAllowed},
      {"disallowed", extensions::api::odfs_config_private::Mount::kDisallowed},
      {"automated", extensions::api::odfs_config_private::Mount::kAutomated},
  };

  for (const auto& test_case : test_cases) {
    SetOneDriveMount(profile(), test_case.policy_value);
    auto function =
        base::MakeRefCounted<extensions::OdfsConfigPrivateGetMountFunction>();
    auto returned_mount_info_value =
        RunFunctionAndReturnValue(function.get(), /*args=*/"[]");

    ASSERT_TRUE(returned_mount_info_value);
    absl::optional<extensions::api::odfs_config_private::MountInfo>
        returned_mount_info =
            extensions::api::odfs_config_private::MountInfo::FromValue(
                *returned_mount_info_value);

    ASSERT_TRUE(returned_mount_info.has_value());
    extensions::api::odfs_config_private::Mount returned_mode =
        returned_mount_info->mode;
    EXPECT_EQ(returned_mode, test_case.expected_mode);
  }
}

TEST_F(OfdsConfigPrivateApiUnittest, GetAccountRestrictionsSuccessful) {
  struct {
    std::vector<std::string> restrictions;
  } test_cases[] = {
      {{"common"}},
      {{"organizations"}},
      {{"https://www.google.com", "abcd1234-1234-1234-1234-1234abcd1234"}},
  };

  for (const auto& test_case : test_cases) {
    SetOneDriveAccountRestrictions(profile(), test_case.restrictions);
    auto function = base::MakeRefCounted<
        extensions::OdfsConfigPrivateGetAccountRestrictionsFunction>();
    auto returned_restrictions_value =
        RunFunctionAndReturnValue(function.get(), /*args=*/"[]");

    ASSERT_TRUE(returned_restrictions_value);
    absl::optional<
        extensions::api::odfs_config_private::AccountRestrictionsInfo>
        returned_account_restrictions = extensions::api::odfs_config_private::
            AccountRestrictionsInfo::FromValue(*returned_restrictions_value);

    ASSERT_TRUE(returned_account_restrictions.has_value());
    std::vector<std::string> returned_restrictions =
        returned_account_restrictions->restrictions;
    EXPECT_THAT(returned_restrictions,
                testing::ElementsAreArray(test_case.restrictions));
  }
}

}  // namespace extensions
