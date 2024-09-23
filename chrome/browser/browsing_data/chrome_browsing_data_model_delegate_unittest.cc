// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browsing_topics/test_util.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "components/nacl/browser/nacl_browser.h"
#endif  // BUILDFLAG(ENABLE_NACL)

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

namespace {

blink::StorageKey StorageKey1() {
  return blink::StorageKey::CreateFromStringForTesting("https://example.com");
}

blink::StorageKey StorageKey2() {
  return blink::StorageKey::CreateFromStringForTesting("https://example2.com");
}

}  // namespace

#if BUILDFLAG(ENABLE_NACL)
class ScopedNaClBrowserDelegate {
 public:
  ~ScopedNaClBrowserDelegate() { nacl::NaClBrowser::ClearAndDeleteDelegate(); }

  void Init(ProfileManager* profile_manager) {
    nacl::NaClBrowser::SetDelegate(
        std::make_unique<NaClBrowserDelegateImpl>(profile_manager));
  }
};
#endif  // BUILDFLAG(ENABLE_NACL)

class ChromeBrowsingDataModelDelegateTest : public testing::Test {
 public:
  ChromeBrowsingDataModelDelegateTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
              media_device_salt::kMediaDeviceIdPartitioning
        },
        /*disabled_features=*/{});
  }

  ChromeBrowsingDataModelDelegateTest(
      const ChromeBrowsingDataModelDelegateTest&) = delete;
  ChromeBrowsingDataModelDelegateTest& operator=(
      const ChromeBrowsingDataModelDelegateTest&) = delete;

  ~ChromeBrowsingDataModelDelegateTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp(temp_dir_.GetPath()));
    profile_ = profile_manager_->CreateTestingProfile("test_profile");

    delegate_ = ChromeBrowsingDataModelDelegate::CreateForProfile(profile_);

    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindLambdaForTesting([this](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              auto mock_browsing_topics_service = std::make_unique<
                  browsing_topics::MockBrowsingTopicsService>();
              mock_browsing_topics_service_ =
                  mock_browsing_topics_service.get();
              return mock_browsing_topics_service;
            }));

#if BUILDFLAG(ENABLE_NACL)
    // Clearing Cache will clear PNACL cache, which needs this delegate set.
    nacl_browser_delegate_.Init(profile_manager_->profile_manager());
#endif  // BUILDFLAG(ENABLE_NACL)

#if !BUILDFLAG(IS_ANDROID)
    if (auto* web_app_provider =
            web_app::WebAppProvider::GetForWebApps(profile_.get())) {
      web_app_provider->command_manager().Start();
    }
#endif

    media_device_salt_service_ =
        MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
            profile_.get());
    // Get salts for test keys, so that they are stored in the service.
    base::test::TestFuture<const std::string&> future;
    media_device_salt_service()->GetSalt(StorageKey1(), future.GetCallback());
    ASSERT_TRUE(future.Wait());
    future.Clear();
    media_device_salt_service()->GetSalt(StorageKey2(), future.GetCallback());
    ASSERT_TRUE(future.Wait());

    base::test::TestFuture<std::vector<blink::StorageKey>> all_keys_future;
    media_device_salt_service()->GetAllStorageKeys(
        all_keys_future.GetCallback());
    ASSERT_THAT(all_keys_future.Get(),
                UnorderedElementsAre(StorageKey1(), StorageKey2()));

    federated_identity_permission_context_ =
        FederatedIdentityPermissionContextFactory::GetForProfile(
            profile_.get());
  }

  TestingProfile* profile() { return profile_.get(); }

  ChromeBrowsingDataModelDelegate* delegate() { return delegate_.get(); }

  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return mock_browsing_topics_service_;
  }

  media_device_salt::MediaDeviceSaltService* media_device_salt_service() {
    return media_device_salt_service_;
  }

  FederatedIdentityPermissionContext* federated_identity_permission_context() {
    return federated_identity_permission_context_;
  }

 protected:
#if BUILDFLAG(ENABLE_NACL)
  ScopedNaClBrowserDelegate nacl_browser_delegate_;
#endif  // BUILDFLAG(ENABLE_NACL)
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;  // Owned by `profile_manager_`.
  raw_ptr<browsing_topics::MockBrowsingTopicsService>
      mock_browsing_topics_service_;
  std::unique_ptr<ChromeBrowsingDataModelDelegate> delegate_;
  raw_ptr<media_device_salt::MediaDeviceSaltService> media_device_salt_service_;
  raw_ptr<FederatedIdentityPermissionContext>
      federated_identity_permission_context_;
};

TEST_F(ChromeBrowsingDataModelDelegateTest, RemoveDataKeyForTopics) {
  auto test_origin = url::Origin::Create(GURL("a.test"));
  EXPECT_CALL(*mock_browsing_topics_service(),
              ClearTopicsDataForOrigin(test_origin))
      .Times(1);

  delegate()->RemoveDataKey(
      test_origin,
      {static_cast<BrowsingDataModel::StorageType>(
          ChromeBrowsingDataModelDelegate::StorageType::kTopics)},
      base::DoNothing());
}

TEST_F(ChromeBrowsingDataModelDelegateTest, RemoveDataKeyForMediaDeviceSalts) {
  base::test::TestFuture<void> done_future;
  delegate()->RemoveDataKey(
      StorageKey1(),
      {static_cast<BrowsingDataModel::StorageType>(
          ChromeBrowsingDataModelDelegate::StorageType::kMediaDeviceSalt)},
      done_future.GetCallback());
  ASSERT_TRUE(done_future.Wait());

  base::test::TestFuture<std::vector<blink::StorageKey>> all_keys_future;
  media_device_salt_service()->GetAllStorageKeys(all_keys_future.GetCallback());
  EXPECT_THAT(all_keys_future.Get(), ElementsAre(StorageKey2()));
}

TEST_F(ChromeBrowsingDataModelDelegateTest, GetAllDataKeysAndGetDataOwner) {
  base::test::TestFuture<
      std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>>
      future;
  delegate()->GetAllDataKeys(future.GetCallback());
  auto delegate_entries = future.Get();

  std::vector<blink::StorageKey> expected_keys = {StorageKey1(), StorageKey2()};
  for (const auto& entry : delegate_entries) {
    const blink::StorageKey* storage_key =
        absl::get_if<blink::StorageKey>(&entry.data_key);
    ASSERT_TRUE(storage_key);
    EXPECT_THAT(expected_keys, Contains(*storage_key));
    std::erase(expected_keys, *storage_key);

    EXPECT_GT(entry.storage_size, 0u);

    EXPECT_EQ(static_cast<ChromeBrowsingDataModelDelegate::StorageType>(
                  entry.storage_type),
              ChromeBrowsingDataModelDelegate::StorageType::kMediaDeviceSalt);

    std::optional<BrowsingDataModel::DataOwner> owner =
        delegate()->GetDataOwner(
            entry.data_key, static_cast<BrowsingDataModel::StorageType>(
                                ChromeBrowsingDataModelDelegate::StorageType::
                                    kMediaDeviceSalt));
    ASSERT_TRUE(owner.has_value());

    const std::string* str_owner = absl::get_if<std::string>(&*owner);
    ASSERT_TRUE(str_owner);
    EXPECT_EQ(*str_owner, storage_key->origin().host());
  }
  EXPECT_TRUE(expected_keys.empty());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeBrowsingDataModelDelegateTest, RemoveIsolatedWebAppData) {
  auto testOrigin = url::Origin::Create(
      GURL("isolated-app://"
           "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"));
  std::unique_ptr<ChromeBrowsingDataModelDelegate> delegate =
      ChromeBrowsingDataModelDelegate::CreateForProfile(profile());
  ASSERT_TRUE(delegate);

  content::BrowsingDataRemover* remover = profile()->GetBrowsingDataRemover();
  ASSERT_EQ(~0ULL, remover->GetLastUsedRemovalMaskForTesting());

  base::RunLoop run_loop;
  delegate->RemoveDataKey(
      testOrigin,
      {static_cast<BrowsingDataModel::StorageType>(
          ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp)},
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ((chrome_browsing_data_remover::DATA_TYPE_SITE_DATA |
             content::BrowsingDataRemover::DATA_TYPE_CACHE) &
                ~content::BrowsingDataRemover::DATA_TYPE_COOKIES,
            remover->GetLastUsedRemovalMaskForTesting());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeBrowsingDataModelDelegateTest, CookieDeletionFilterChildUser) {
  profile_->SetIsSupervisedProfile(true);

  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_TRUE(delegate()->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_TRUE(
      delegate()->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

TEST_F(ChromeBrowsingDataModelDelegateTest, CookieDeletionFilterNormalUser) {
  profile_->SetIsSupervisedProfile(false);

  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

TEST_F(ChromeBrowsingDataModelDelegateTest, CookieDeletionFilterIncognitoUser) {
  // Replace the delegate with an incognito profile delegate.
  delegate_ = ChromeBrowsingDataModelDelegate::CreateForProfile(
      profile_->GetOffTheRecordProfile(Profile::OTRProfileID::PrimaryID(),
                                       /*create_if_needed=*/true));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_FALSE(
      delegate()->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

TEST_F(ChromeBrowsingDataModelDelegateTest, RemoveFederatedIdentityData) {
  const url::Origin kRequester =
      url::Origin::Create(GURL("https://requester.com"));
  const url::Origin kEmbedder =
      url::Origin::Create(GURL("https://embedder.com"));
  const url::Origin kIdentityProvider =
      url::Origin::Create(GURL("https://idp.com"));
  constexpr std::string kAccountId = "accountId";

  FederatedIdentityPermissionContext* context =
      federated_identity_permission_context();
  context->GrantSharingPermission(kRequester, kEmbedder, kIdentityProvider,
                                  kAccountId);
  EXPECT_TRUE(context->GetLastUsedTimestamp(kRequester, kEmbedder,
                                            kIdentityProvider, kAccountId));
  EXPECT_TRUE(context->HasSharingPermission(kRequester));
  EXPECT_FALSE(context->HasSharingPermission(kEmbedder));

  base::RunLoop run_loop;
  delegate_->RemoveDataKey(
      webid::FederatedIdentityDataModel::DataKey(kRequester, kEmbedder,
                                                 kIdentityProvider, kAccountId),
      {static_cast<BrowsingDataModel::StorageType>(
          ChromeBrowsingDataModelDelegate::StorageType::kFederatedIdentity)},
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(context->GetLastUsedTimestamp(kRequester, kEmbedder,
                                             kIdentityProvider, kAccountId));
  EXPECT_FALSE(context->HasSharingPermission(kRequester));
}
