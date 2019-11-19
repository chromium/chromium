// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_cache_unittest.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::Return;
using ::testing::NiceMock;

namespace {

class ProfileDownloaderMock : public ProfileDownloader {
 public:
  explicit ProfileDownloaderMock(ProfileDownloaderDelegate* delegate)
      : ProfileDownloader(delegate) {
  }

  ~ProfileDownloaderMock() override {}

  MOCK_CONST_METHOD0(GetProfileFullName, base::string16());
  MOCK_CONST_METHOD0(GetProfileGivenName, base::string16());
  MOCK_CONST_METHOD0(GetProfilePicture, SkBitmap());
  MOCK_CONST_METHOD0(GetProfilePictureStatus,
                     ProfileDownloader::PictureStatus());
  MOCK_CONST_METHOD0(GetProfilePictureURL, std::string());
  MOCK_CONST_METHOD0(GetProfileHostedDomain, base::string16());
};

class GAIAInfoUpdateServiceMock : public GAIAInfoUpdateService {
 public:
  explicit GAIAInfoUpdateServiceMock(Profile* profile)
      : GAIAInfoUpdateService(profile) {
  }

  ~GAIAInfoUpdateServiceMock() override {}

  MOCK_METHOD0(Update, void());
};

// TODO(anthonyvd) : remove ProfileInfoCacheTest from the test fixture.
class GAIAInfoUpdateServiceTestBase : public ProfileInfoCacheTest {
 protected:
  explicit GAIAInfoUpdateServiceTestBase(bool create_gaia_info_service_on_setup)
      : create_gaia_info_service_on_setup_(create_gaia_info_service_on_setup) {}
  ~GAIAInfoUpdateServiceTestBase() override = default;

  void SetUp() override {
    ProfileInfoCacheTest::SetUp();
    if (create_gaia_info_service_on_setup_) {
      service_.reset(new NiceMock<GAIAInfoUpdateServiceMock>(profile()));
      downloader_.reset(new NiceMock<ProfileDownloaderMock>(service()));
    }

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

  void TearDown() override {
    if (downloader_)
      downloader_.reset();
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
    ProfileInfoCacheTest::TearDown();
  }

  Profile* profile() {
    if (!profile_)
      profile_ = CreateProfile("Person 1");
    return profile_;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  ProfileAttributesStorage* storage() {
    return testing_profile_manager_.profile_attributes_storage();
  }

  NiceMock<GAIAInfoUpdateServiceMock>* service() { return service_.get(); }
  NiceMock<ProfileDownloaderMock>* downloader() { return downloader_.get(); }

  Profile* CreateProfile(const std::string& name) {
    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    testing_factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    Profile* profile = testing_profile_manager_.CreateTestingProfile(
        name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(name), 0, std::string(),
        std::move(testing_factories));
    // The testing manager sets the profile name manually, which counts as
    // a user-customized profile name. Reset this to match the default name
    // we are actually using.
    ProfileAttributesEntry* entry = nullptr;
    // TODO(anthonyvd) : refactor the function so the following assertion can be
    // changed to ASSERT_TRUE.
    EXPECT_TRUE(storage()->GetProfileAttributesWithPath(profile->GetPath(),
                                                        &entry));
    entry->SetIsUsingDefaultName(true);
    return profile;
  }

  static std::string GivenName(const std::string& id) {
    return id + "first";
  }
  static std::string FullName(const std::string& id) {
    return GivenName(id) + " " + id + "last";
  }
  static base::string16 GivenName16(const std::string& id) {
    return base::UTF8ToUTF16(GivenName(id));
  }
  static base::string16 FullName16(const std::string& id) {
    return base::UTF8ToUTF16(FullName(id));
  }

  void ProfileDownloadSuccess(
      const base::string16& full_name,
      const base::string16& given_name,
      const gfx::Image& image,
      const std::string& url,
      const base::string16& hosted_domain) {
    EXPECT_CALL(*downloader(), GetProfileFullName()).
        WillOnce(Return(full_name));
    EXPECT_CALL(*downloader(), GetProfileGivenName()).
        WillOnce(Return(given_name));
    const SkBitmap* bmp = image.ToSkBitmap();
    EXPECT_CALL(*downloader(), GetProfilePicture()).WillOnce(Return(*bmp));
    EXPECT_CALL(*downloader(), GetProfilePictureStatus()).
        WillOnce(Return(ProfileDownloader::PICTURE_SUCCESS));
    EXPECT_CALL(*downloader(), GetProfilePictureURL()).WillOnce(Return(url));
    EXPECT_CALL(*downloader(), GetProfileHostedDomain()).
        WillOnce(Return(hosted_domain));

    service()->OnProfileDownloadSuccess(downloader());
  }

  void RenameProfile(const base::string16& full_name,
                     const base::string16& given_name) {
    gfx::Image image = gfx::test::CreateImage(256, 256);
    std::string url("foo.com");
    ProfileDownloadSuccess(full_name, given_name, image, url, base::string16());

    // Make sure the right profile was updated correctly.
    ProfileAttributesEntry* entry;
    ASSERT_TRUE(
        storage()->GetProfileAttributesWithPath(profile()->GetPath(), &entry));
    EXPECT_EQ(full_name, entry->GetGAIAName());
    EXPECT_EQ(given_name, entry->GetGAIAGivenName());
  }

  const bool create_gaia_info_service_on_setup_;
  Profile* profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<NiceMock<GAIAInfoUpdateServiceMock>> service_;
  std::unique_ptr<NiceMock<ProfileDownloaderMock>> downloader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateServiceTestBase);
};

class GAIAInfoUpdateServiceTest : public GAIAInfoUpdateServiceTestBase {
 public:
  GAIAInfoUpdateServiceTest()
      : GAIAInfoUpdateServiceTestBase(
            /*create_gaia_info_service_on_setup_=*/true) {}
  ~GAIAInfoUpdateServiceTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateServiceTest);
};

class GAIAInfoUpdateServiceMiscTest : public GAIAInfoUpdateServiceTestBase,
                                      public ProfileInfoCacheObserver {
 public:
  GAIAInfoUpdateServiceMiscTest()
      : GAIAInfoUpdateServiceTestBase(
            /*create_gaia_info_service_on_setup_=*/false) {}
  ~GAIAInfoUpdateServiceMiscTest() override = default;

  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override {
    profile_name_changed_count_++;
  }

  void OnProfileAvatarChanged(const base::FilePath& profile_path) override {
    profile_avatar_changed_count_++;
  }

 protected:
  unsigned int profile_name_changed_count_ = 0;
  unsigned int profile_avatar_changed_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateServiceMiscTest);
};

}  // namespace

TEST_F(GAIAInfoUpdateServiceTest, DownloadSuccess) {
  // No URL should be cached yet.
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());
  EXPECT_EQ(std::string(), profile()->GetPrefs()->
      GetString(prefs::kGoogleServicesHostedDomain));

  base::string16 name = base::ASCIIToUTF16("Pat Smith");
  base::string16 given_name = base::ASCIIToUTF16("Pat");
  gfx::Image image = gfx::test::CreateImage(256, 256);
  std::string url("foo.com");
  base::string16 hosted_domain(base::ASCIIToUTF16(""));
  ProfileDownloadSuccess(name, given_name, image, url, hosted_domain);

  // On success the GAIA info should be updated.
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(profile()->GetPath(),
                                                      &entry));
  EXPECT_EQ(name, entry->GetGAIAName());
  EXPECT_EQ(given_name, entry->GetGAIAGivenName());
  EXPECT_TRUE(gfx::test::AreImagesEqual(image, *entry->GetGAIAPicture()));
  EXPECT_EQ(url, service()->GetCachedPictureURL());
  EXPECT_EQ(kNoHostedDomainFound, profile()->GetPrefs()->GetString(
                                      prefs::kGoogleServicesHostedDomain));
}

TEST_F(GAIAInfoUpdateServiceTest, DownloadFailure) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(profile()->GetPath(),
                                                      &entry));
  base::string16 old_name = entry->GetName();
  gfx::Image old_image = entry->GetAvatarIcon();

  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());

  service()->OnProfileDownloadFailure(downloader(),
                                      ProfileDownloaderDelegate::SERVICE_ERROR);

  // On failure nothing should be updated.
  EXPECT_EQ(old_name, entry->GetName());
  EXPECT_EQ(base::string16(), entry->GetGAIAName());
  EXPECT_EQ(base::string16(), entry->GetGAIAGivenName());
  EXPECT_TRUE(gfx::test::AreImagesEqual(old_image, entry->GetAvatarIcon()));
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());
  EXPECT_EQ(std::string(),
      profile()->GetPrefs()->GetString(prefs::kGoogleServicesHostedDomain));
}

TEST_F(GAIAInfoUpdateServiceTest, ProfileLockEnabledForWhitelist) {
  // No URL should be cached yet.
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());

  base::string16 name = base::ASCIIToUTF16("Pat Smith");
  base::string16 given_name = base::ASCIIToUTF16("Pat");
  gfx::Image image = gfx::test::CreateImage(256, 256);
  std::string url("foo.com");
  base::string16 hosted_domain(base::ASCIIToUTF16("google.com"));
  ProfileDownloadSuccess(name, given_name, image, url, hosted_domain);

  EXPECT_EQ("google.com", profile()->GetPrefs()->
      GetString(prefs::kGoogleServicesHostedDomain));
}

TEST_F(GAIAInfoUpdateServiceTest, ShouldUseGAIAProfileInfo) {
#if defined(OS_CHROMEOS)
  // This feature should never be enabled on ChromeOS.
  EXPECT_FALSE(GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(profile()));
#endif
}

TEST_F(GAIAInfoUpdateServiceTest, ScheduleUpdate) {
  EXPECT_TRUE(service()->timer_.IsRunning());
  service()->timer_.Stop();
  EXPECT_FALSE(service()->timer_.IsRunning());
  service()->ScheduleNextUpdate();
  EXPECT_TRUE(service()->timer_.IsRunning());
}

#if !defined(OS_CHROMEOS)

TEST_F(GAIAInfoUpdateServiceTest, LogOut) {
  identity_test_env()->SetPrimaryAccount("pat@example.com");
  base::string16 gaia_name = base::UTF8ToUTF16("Pat Foo");

  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  entry->SetGAIAName(gaia_name);
  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  entry->SetGAIAPicture(gaia_picture);

  // Set a fake picture URL.
  profile()->GetPrefs()->SetString(prefs::kProfileGAIAInfoPictureURL,
                                   "example.com");

  EXPECT_FALSE(service()->GetCachedPictureURL().empty());

  // Log out.
  identity_test_env()->ClearPrimaryAccount();
  // Verify that the GAIA name and picture, and picture URL are unset.
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_TRUE(service()->GetCachedPictureURL().empty());
}

TEST_F(GAIAInfoUpdateServiceTest, LogIn) {
  // Log in.
  EXPECT_CALL(*service(), Update()).Times(testing::AtLeast(1));
  identity_test_env()->SetPrimaryAccount("pat@example.com");
}

TEST_F(GAIAInfoUpdateServiceMiscTest, ClearGaiaInfoOnStartup) {
  // Simulate a state where the profile entry has GAIA related information
  // when there is not primary account set.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount());
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  entry->SetGAIAName(base::UTF8ToUTF16("foo"));
  entry->SetGAIAGivenName(base::UTF8ToUTF16("Pat Foo"));
  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  entry->SetGAIAPicture(gaia_picture);

  GetCache()->AddObserver(this);

  // Verify that creating the GAIAInfoUpdateService resets the GAIA related
  // profile attributes if the profile no longer has a primary account and that
  // the profile info cache observer wass notified about profile name and
  // avatar changes.
  service_.reset(new NiceMock<GAIAInfoUpdateServiceMock>(profile()));
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_FALSE(entry->GetGAIAPicture());
  EXPECT_EQ(1U, profile_name_changed_count_);
  EXPECT_EQ(1U, profile_avatar_changed_count_);

  GetCache()->RemoveObserver(this);
}

// Regression test for http://crbug.com/900374
TEST_F(GAIAInfoUpdateServiceMiscTest,
       ClearGaiaInfoForSignedOutProfileDoesNotNotifyProfileObservers) {
  // Simulate a state where the profile entry has no GAIA related information
  // and when there is not primary account set.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount());
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  ASSERT_TRUE(entry->GetGAIAName().empty());
  ASSERT_TRUE(entry->GetGAIAGivenName().empty());
  ASSERT_FALSE(entry->GetGAIAPicture());

  GetCache()->AddObserver(this);

  // Verify that the state for the profile entry did not change and that the
  // profile info cache observer was not notified about any profile name
  // and avatar changes.
  service_.reset(new NiceMock<GAIAInfoUpdateServiceMock>(profile()));
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_FALSE(entry->GetGAIAPicture());
  EXPECT_EQ(0U, profile_name_changed_count_);
  EXPECT_EQ(0U, profile_avatar_changed_count_);

  GetCache()->RemoveObserver(this);
}

#endif
