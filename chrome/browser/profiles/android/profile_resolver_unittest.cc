// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/android/profile_resolver.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "chrome/browser/android/proto/profile_token.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profile_resolver {

class ProfileResolverTest : public testing::Test {
 public:
  ProfileResolverTest() : manager_(TestingBrowserProcess::GetGlobal()) {}

  TestingProfileManager* manager() { return &manager_; }

  void SetUp() override {
    // Otherwise we see a /data/data vs /data/user/ mismatch.
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    ASSERT_TRUE(manager_.SetUp(user_data_dir));
  }

  Profile* ResolveProfileSync(std::string token) {
    Profile* profile;
    base::RunLoop run_loop;
    ResolveProfile(token, base::BindLambdaForTesting([&](Profile* p) {
                     profile = p;
                     run_loop.Quit();
                   }));
    run_loop.Run();
    return profile;
  }

  ProfileKey* ResolveProfileKeySync(std::string token) {
    ProfileKey* profile_key;
    base::RunLoop run_loop;
    ResolveProfileKey(token, base::BindLambdaForTesting([&](ProfileKey* pk) {
                        profile_key = pk;
                        run_loop.Quit();
                      }));
    run_loop.Run();
    return profile_key;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;
};

TEST_F(ProfileResolverTest, TestTokenizeResolve) {
  TestingProfile* profile = manager()->CreateTestingProfile("foo");
  std::string token = TokenizeProfile(profile);
  Profile* resolved_profile = ResolveProfileSync(token);
  ASSERT_EQ(profile, resolved_profile);

  std::string another_token = TokenizeProfile(profile);
  ASSERT_EQ(token, another_token);
}

TEST_F(ProfileResolverTest, TestResolveDifferentProfiles) {
  TestingProfile* profile1 = manager()->CreateTestingProfile("foo");
  TestingProfile* profile2 = manager()->CreateTestingProfile("bar");

  std::string token1 = TokenizeProfile(profile1);
  std::string token2 = TokenizeProfile(profile2);
  ASSERT_NE(token1, token2);

  Profile* resolved_profile1 = ResolveProfileSync(token1);
  Profile* resolved_profile2 = ResolveProfileSync(token2);
  ASSERT_EQ(profile1, resolved_profile1);
  ASSERT_EQ(profile2, resolved_profile2);
  ASSERT_NE(resolved_profile1, resolved_profile2);
}

TEST_F(ProfileResolverTest, TestResolveOtrProfiles) {
  TestingProfile* profile = manager()->CreateTestingProfile("foo");
  Profile* primaryOtrProfile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Profile* anotherOtrProfile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUnique("Test:Foo"),
      /*create_if_needed=*/true);

  std::string token = TokenizeProfile(profile);
  std::string primaryOtrToken = TokenizeProfile(primaryOtrProfile);
  std::string anotherOtrToken = TokenizeProfile(anotherOtrProfile);
  ASSERT_NE(token, primaryOtrToken);
  ASSERT_NE(token, anotherOtrToken);
  ASSERT_NE(primaryOtrToken, anotherOtrToken);

  Profile* resolved_primary_otr_profile = ResolveProfileSync(primaryOtrToken);
  Profile* another_primary_otr_profile = ResolveProfileSync(anotherOtrToken);
  ASSERT_EQ(primaryOtrProfile, resolved_primary_otr_profile);
  ASSERT_EQ(anotherOtrProfile, another_primary_otr_profile);
}

TEST_F(ProfileResolverTest, TestResolveProfileKeys) {
  ProfileKey* profile_key1 =
      manager()->CreateTestingProfile("foo")->GetProfileKey();
  ProfileKey* profile_key2 =
      manager()->CreateTestingProfile("bar")->GetProfileKey();

  std::string token1 = TokenizeProfileKey(profile_key1);
  std::string token2 = TokenizeProfileKey(profile_key2);
  ASSERT_NE(token1, token2);

  ProfileKey* resolved_profile_key1 = ResolveProfileKeySync(token1);
  ProfileKey* resolved_profile_key2 = ResolveProfileKeySync(token2);
  ASSERT_EQ(profile_key1, resolved_profile_key1);
  ASSERT_EQ(profile_key2, resolved_profile_key2);
  ASSERT_NE(resolved_profile_key1, resolved_profile_key2);
}

TEST_F(ProfileResolverTest, TestResolveBadToken) {
  ProfileToken token_proto;
  token_proto.set_relative_path("foo");
  std::string token_string;
  token_proto.SerializeToString(&token_string);

  ASSERT_EQ(nullptr, ResolveProfileSync(token_string));
  ASSERT_EQ(nullptr, ResolveProfileKeySync(token_string));
}

TEST_F(ProfileResolverTest, TestResolveBadOtrToken) {
  ProfileToken otr_token_proto;
  otr_token_proto.set_relative_path("bar");
  otr_token_proto.set_otr_profile_id(
      Profile::OTRProfileID::CreateUnique("Test:Bar").Serialize());
  std::string otr_token_string;
  otr_token_proto.SerializeToString(&otr_token_string);

  ASSERT_EQ(nullptr, ResolveProfileSync(otr_token_string));
  ASSERT_EQ(nullptr, ResolveProfileKeySync(otr_token_string));
}

TEST_F(ProfileResolverTest, TestResolveUnloadedProfile) {
  TestingProfile* profile = manager()->CreateTestingProfile("foo");
  ProfileKey* profile_key = profile->GetProfileKey();
  Profile* primary_otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  std::string profile_token = TokenizeProfile(profile);
  std::string profile_key_token = TokenizeProfileKey(profile_key);
  std::string primary_otr_profile_token = TokenizeProfile(primary_otr_profile);

  // TODO(alexilin): Call some sort of UnloadProfile() mechanism on
  // TestingProfileManager to remove these profiles from memory but leave the
  // metadata.

  // Our old raw pointers are all now invalid and should not be used again.
  profile = nullptr;
  profile_key = nullptr;
  primary_otr_profile = nullptr;

  Profile* resolved_profile = ResolveProfileSync(profile_token);
  ProfileKey* resolved_profile_key = ResolveProfileKeySync(profile_key_token);
  Profile* resolved_primary_otr_profile =
      ResolveProfileSync(primary_otr_profile_token);
  ASSERT_NE(nullptr, resolved_profile);
  ASSERT_NE(nullptr, resolved_profile_key);
  ASSERT_NE(nullptr, resolved_primary_otr_profile);
}

}  // namespace profile_resolver
