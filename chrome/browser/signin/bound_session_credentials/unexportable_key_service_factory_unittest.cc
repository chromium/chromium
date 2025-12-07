// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/test/base/testing_profile.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

class UnexportableKeyServiceFactoryTest : public testing::Test {
 public:
  UnexportableKeyServiceFactoryTest() {
    UnexportableKeyServiceFactory::GetInstance()->SetServiceFactoryForTesting(
        base::BindRepeating(
            &UnexportableKeyServiceFactoryTest::CreateFakeService,
            base::Unretained(this)));
  }

  ~UnexportableKeyServiceFactoryTest() override {
    UnexportableKeyServiceFactory::GetInstance()->SetServiceFactoryForTesting(
        base::NullCallback());
  }

  unexportable_keys::UnexportableKeyService* GetServiceForProfileAndPurpose(
      Profile* profile,
      UnexportableKeyServiceFactory::KeyPurpose purpose) {
    return UnexportableKeyServiceFactory::GetForProfileAndPurpose(profile,
                                                                  purpose);
  }

  crypto::UnexportableKeyProvider::Config GetConfigForProfileAndPurpose(
      Profile* profile,
      UnexportableKeyServiceFactory::KeyPurpose purpose) {
    return service_configs_.at(
        UnexportableKeyServiceFactory::GetForProfileAndPurpose(profile,
                                                               purpose));
  }

  std::unique_ptr<unexportable_keys::UnexportableKeyService> CreateFakeService(
      crypto::UnexportableKeyProvider::Config config) {
    auto service =
        std::make_unique<unexportable_keys::FakeUnexportableKeyService>();
    service_configs_.insert({service.get(), std::move(config)});
    return service;
  }

 private:
  absl::flat_hash_map<unexportable_keys::UnexportableKeyService*,
                      crypto::UnexportableKeyProvider::Config>
      service_configs_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UnexportableKeyServiceFactoryTest, DifferentProfiles) {
  TestingProfile profile1;
  TestingProfile profile2;

  const unexportable_keys::UnexportableKeyService* service1 =
      GetServiceForProfileAndPurpose(&profile1,
                                     UnexportableKeyServiceFactory::KeyPurpose::
                                         kDeviceBoundSessionCredentials);
  const unexportable_keys::UnexportableKeyService* service2 =
      GetServiceForProfileAndPurpose(&profile2,
                                     UnexportableKeyServiceFactory::KeyPurpose::
                                         kDeviceBoundSessionCredentials);

  // The services are not null and they are different. Also
  // GetServiceForProfileAndPurpose() is idempotent for the same profile and
  // purpose.
  EXPECT_NE(service1, nullptr);
  EXPECT_NE(service2, nullptr);
  EXPECT_NE(service1, service2);
  EXPECT_EQ(service1, GetServiceForProfileAndPurpose(
                          &profile1, UnexportableKeyServiceFactory::KeyPurpose::
                                         kDeviceBoundSessionCredentials));
  EXPECT_EQ(service2, GetServiceForProfileAndPurpose(
                          &profile2, UnexportableKeyServiceFactory::KeyPurpose::
                                         kDeviceBoundSessionCredentials));

#if BUILDFLAG(IS_MAC)
  const crypto::UnexportableKeyProvider::Config config1 =
      GetConfigForProfileAndPurpose(&profile1,
                                    UnexportableKeyServiceFactory::KeyPurpose::
                                        kDeviceBoundSessionCredentials);
  const crypto::UnexportableKeyProvider::Config config2 =
      GetConfigForProfileAndPurpose(&profile2,
                                    UnexportableKeyServiceFactory::KeyPurpose::
                                        kDeviceBoundSessionCredentials);

  EXPECT_EQ(UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
            config1.keychain_access_group);
  EXPECT_EQ(UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
            config2.keychain_access_group);
  EXPECT_NE(config1.application_tag, config2.application_tag);
#endif  // BUILDFLAG(IS_MAC)
}

TEST_F(UnexportableKeyServiceFactoryTest, DifferentPurposes) {
  TestingProfile profile;
  const unexportable_keys::UnexportableKeyService* lst_service =
      GetServiceForProfileAndPurpose(
          &profile,
          UnexportableKeyServiceFactory::KeyPurpose::kRefreshTokenBinding);
  const unexportable_keys::UnexportableKeyService* dbsc_service =
      GetServiceForProfileAndPurpose(&profile,
                                     UnexportableKeyServiceFactory::KeyPurpose::
                                         kDeviceBoundSessionCredentials);

  // The services are not null and they are different. Also
  // GetServiceForProfileAndPurpose() is idempotent for the same profile and
  // purpose.
  EXPECT_NE(lst_service, nullptr);
  EXPECT_NE(dbsc_service, nullptr);
  EXPECT_NE(lst_service, dbsc_service);
  EXPECT_EQ(
      lst_service,
      GetServiceForProfileAndPurpose(
          &profile,
          UnexportableKeyServiceFactory::KeyPurpose::kRefreshTokenBinding));
  EXPECT_EQ(dbsc_service,
            GetServiceForProfileAndPurpose(
                &profile, UnexportableKeyServiceFactory::KeyPurpose::
                              kDeviceBoundSessionCredentials));

#if BUILDFLAG(IS_MAC)
  const crypto::UnexportableKeyProvider::Config lst_config =
      GetConfigForProfileAndPurpose(
          &profile,
          UnexportableKeyServiceFactory::KeyPurpose::kRefreshTokenBinding);
  const crypto::UnexportableKeyProvider::Config dbsc_config =
      GetConfigForProfileAndPurpose(&profile,
                                    UnexportableKeyServiceFactory::KeyPurpose::
                                        kDeviceBoundSessionCredentials);

  EXPECT_EQ(UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
            lst_config.keychain_access_group);
  EXPECT_EQ(UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
            dbsc_config.keychain_access_group);
  EXPECT_NE(lst_config.application_tag, dbsc_config.application_tag);
  EXPECT_TRUE(lst_config.application_tag.ends_with("lst"));
  EXPECT_TRUE(dbsc_config.application_tag.ends_with("dbsc"));
#endif  // BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(IS_MAC)
TEST_F(UnexportableKeyServiceFactoryTest, CorrectApplicationTag) {
  TestingProfile profile(base::FilePath("/user/data/dir/test_profile"));
  profile.SetCreationTimeForTesting(base::Time::UnixEpoch());

  const crypto::UnexportableKeyProvider::Config config =
      GetConfigForProfileAndPurpose(&profile,
                                    UnexportableKeyServiceFactory::KeyPurpose::
                                        kDeviceBoundSessionCredentials);

  EXPECT_EQ(config.application_tag,
            base::JoinString(
                {
                    UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
                    "af935f0dbf2111a4",  // hex(sha256("/user/data/dir"))[:16]
                    "test_profile",
                    "af5570f5a1810b7a",  // hex(sha256(u64{0}))[:16]
                    "dbsc",
                },
                "."));
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace
