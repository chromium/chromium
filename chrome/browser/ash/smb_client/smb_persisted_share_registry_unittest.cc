// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_persisted_share_registry.h"

#include "chrome/browser/ash/smb_client/smb_url.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {
namespace {

const char kShareUrl[] = "smb://server/share1";
const char kShareUrl2[] = "smb://server/share2";
const char kDisplayName[] = "My File Share";
const char kUsername[] = "test-user";
const char kUsername2[] = "test-user2";
const char kWorkgroup[] = "test-workgroup.com";

class SmbPersistedShareRegistryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SmbPersistedShareRegistryTest, Empty) {
  SmbPersistedShareRegistry registry(&profile_);
  std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  EXPECT_FALSE(info);

  std::vector<SmbShareInfo> all_info = registry.GetAll();
  EXPECT_TRUE(all_info.empty());

  // Should do nothing, not crash.
  registry.Delete(SmbUrl(kShareUrl));
}

TEST_F(SmbPersistedShareRegistryTest, SaveGet) {
  // Use a local const vector because SmbShareInfo() takes the salt as a
  // vector<>.
  const std::vector<uint8_t> kSalt = {1, 2, 9, 0, 'a', 'b', 0, 255};
  {
    SmbPersistedShareRegistry registry(&profile_);
    SmbShareInfo info1(SmbUrl(kShareUrl), kDisplayName, kUsername, kWorkgroup,
                       false /* use_kerberos */, kSalt);
    registry.Save(info1);

    SmbShareInfo info2(SmbUrl(kShareUrl2), kDisplayName, kUsername2, kWorkgroup,
                       true /* use_kerberos */);
    registry.Save(info2);
  }
  // Use scopes to simulate a logout/login so that the instances of
  // SmbPersistedShareRegistry are not the same (and have no hidden state).
  {
    SmbPersistedShareRegistry registry(&profile_);
    std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->share_url().ToString(), kShareUrl);
    EXPECT_EQ(info->display_name(), kDisplayName);
    EXPECT_EQ(info->username(), kUsername);
    EXPECT_EQ(info->workgroup(), kWorkgroup);
    EXPECT_FALSE(info->use_kerberos());
    EXPECT_EQ(info->password_salt(), kSalt);

    std::optional<SmbShareInfo> info2 = registry.Get(SmbUrl(kShareUrl2));
    ASSERT_TRUE(info2);
    EXPECT_EQ(info2->share_url().ToString(), kShareUrl2);
    EXPECT_EQ(info2->display_name(), kDisplayName);
    EXPECT_EQ(info2->username(), kUsername2);
    EXPECT_EQ(info2->workgroup(), kWorkgroup);
    EXPECT_TRUE(info2->use_kerberos());
    EXPECT_TRUE(info2->password_salt().empty());

    std::vector<SmbShareInfo> all_info = registry.GetAll();
    EXPECT_EQ(all_info.size(), 2u);
    EXPECT_EQ(all_info[0].share_url().ToString(), kShareUrl);
    EXPECT_EQ(all_info[1].share_url().ToString(), kShareUrl2);
  }
}

TEST_F(SmbPersistedShareRegistryTest, Replace) {
  {
    SmbPersistedShareRegistry registry(&profile_);
    SmbShareInfo info(SmbUrl(kShareUrl), kDisplayName, kUsername, kWorkgroup,
                      false /* use_kerberos */);
    registry.Save(info);
  }
  // Use scopes to simulate a logout/login so that the instances of
  // SmbPersistedShareRegistry are not the same (and have no hidden state).
  {
    SmbPersistedShareRegistry registry(&profile_);
    std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->share_url().ToString(), kShareUrl);
    EXPECT_EQ(info->display_name(), kDisplayName);
    EXPECT_EQ(info->username(), kUsername);
    EXPECT_EQ(info->workgroup(), kWorkgroup);
    EXPECT_FALSE(info->use_kerberos());

    std::vector<SmbShareInfo> all_info = registry.GetAll();
    EXPECT_EQ(all_info.size(), 1u);

    SmbShareInfo replace_info(SmbUrl(kShareUrl), kDisplayName, kUsername2,
                              kWorkgroup, true /* use_kerberos */);
    registry.Save(replace_info);
  }
  {
    SmbPersistedShareRegistry registry(&profile_);
    std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->share_url().ToString(), kShareUrl);
    EXPECT_EQ(info->display_name(), kDisplayName);
    EXPECT_EQ(info->username(), kUsername2);
    EXPECT_EQ(info->workgroup(), kWorkgroup);
    EXPECT_TRUE(info->use_kerberos());

    std::vector<SmbShareInfo> all_info = registry.GetAll();
    EXPECT_EQ(all_info.size(), 1u);
  }
}

TEST_F(SmbPersistedShareRegistryTest, Delete) {
  {
    SmbPersistedShareRegistry registry(&profile_);
    SmbShareInfo info1(SmbUrl(kShareUrl), kDisplayName, kUsername, kWorkgroup,
                       false /* use_kerberos */);
    registry.Save(info1);

    SmbShareInfo info2(SmbUrl(kShareUrl2), kDisplayName, kUsername2, kWorkgroup,
                       true /* use_kerberos */);
    registry.Save(info2);
  }
  // Use scopes to simulate a logout/login so that the instances of
  // SmbPersistedShareRegistry are not the same (and have no hidden state).
  {
    SmbPersistedShareRegistry registry(&profile_);
    registry.Delete(SmbUrl(kShareUrl2));

    std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->share_url().ToString(), kShareUrl);
    EXPECT_EQ(info->display_name(), kDisplayName);
    EXPECT_EQ(info->username(), kUsername);
    EXPECT_EQ(info->workgroup(), kWorkgroup);
    EXPECT_FALSE(info->use_kerberos());

    std::optional<SmbShareInfo> info2 = registry.Get(SmbUrl(kShareUrl2));
    ASSERT_FALSE(info2);

    std::vector<SmbShareInfo> all_info = registry.GetAll();
    EXPECT_EQ(all_info.size(), 1u);
  }
}

}  // namespace
}  // namespace ash::smb_client
