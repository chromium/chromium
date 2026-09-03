// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ntp_customization/ntp_android_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_synced_theme_bridge.h"
#include "chrome/browser/ntp_customization/ntp_theme_collection_bridge.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/themes/ntp_background_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTestBackgroundUrl[] = "https://example.com/bg.png";
constexpr char kTestThumbnailUrl[] = "https://example.com/thumb.png";
constexpr char kTestAttribution1[] = "Attribution 1";
constexpr char kTestAttribution2[] = "Attribution 2";
constexpr char kTestActionUrl[] = "https://example.com/action";
constexpr char kTestCollectionId[] = "collection_id";
constexpr char kTestInvalidUrl[] = "foo";
constexpr char kTestPrefUrl[] = "https://example.com/pref.png";
constexpr char kTestSomeId[] = "some_id";
constexpr char kTestBackdropCollectionId[] = "backdrop_collection";
constexpr char kTestValidUrl[] = "https://example.com/valid.png";
constexpr char kTestValidUrl2[] = "https://example.com/2.png";
constexpr char kTestCollectionIdA[] = "collection_A";
constexpr char kAndroidThemeStorageKey[] = "current_android_theme";

class MockThemeCollectionBridge : public NtpThemeCollectionBridge {
 public:
  MockThemeCollectionBridge() = default;
  ~MockThemeCollectionBridge() override = default;
  MOCK_METHOD(void, OnCustomBackgroundImageUpdated, (), (override));
};

class MockSyncedThemeBridge : public NtpSyncedThemeBridge {
 public:
  MockSyncedThemeBridge() = default;
  ~MockSyncedThemeBridge() override = default;
  MOCK_METHOD(void, OnCustomBackgroundImageUpdated, (), (override));
};

class MockObserver : public NtpCustomBackgroundServiceObserver {
 public:
  MOCK_METHOD(void, OnCustomBackgroundImageUpdated, (), (override));
  MOCK_METHOD(void, OnNtpCustomBackgroundServiceShuttingDown, (), (override));
};

class MockNtpBackgroundService : public NtpBackgroundService {
 public:
  MockNtpBackgroundService(
      ApplicationLocaleStorage* locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : NtpBackgroundService(locale_storage, url_loader_factory) {}
  MOCK_METHOD(bool,
              IsValidBackdropCollection,
              (const std::string&),
              (const, override));
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      NtpAndroidBackgroundServiceFactory::GetInstance(),
      base::BindRepeating(
          [](ApplicationLocaleStorage* application_locale_storage,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<MockNtpBackgroundService>>(
                application_locale_storage, url_loader_factory);
          },
          application_locale_storage, url_loader_factory));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  return profile_builder.Build();
}

class NtpAndroidCustomBackgroundServiceTest : public testing::Test {
 protected:
  NtpAndroidCustomBackgroundServiceTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}
  ~NtpAndroidCustomBackgroundServiceTest() override = default;

  void SetUp() override {
    profile_ = MakeTestingProfile(
        &locale_storage_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    mock_background_service_ = static_cast<MockNtpBackgroundService*>(
        NtpAndroidBackgroundServiceFactory::GetForProfile(profile_.get()));

    service_ = std::make_unique<NtpAndroidCustomBackgroundService>(
        profile_.get(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
    service_->AddObserver(&observer_);

    mock_background_service_->AddValidBackdropUrlForTesting(
        GURL(kTestValidUrl));
    mock_background_service_->AddValidBackdropUrlForTesting(
        GURL(kTestValidUrl2));
    EXPECT_CALL(*mock_background_service_,
                IsValidBackdropCollection(testing::_))
        .WillRepeatedly(testing::Return(true));
  }

  void TearDown() override { service_->RemoveObserver(&observer_); }

  sync_pb::ThemeAndroidSpecifics CreateTestThemeSpecifics(
      const std::string& url = kTestValidUrl,
      const std::string& collection_id = kTestCollectionId) {
    sync_pb::ThemeAndroidSpecifics specifics;
    specifics.mutable_ntp_background()->set_url(url);
    specifics.mutable_ntp_background()->set_collection_id(collection_id);
    return specifics;
  }

  content::BrowserTaskEnvironment task_environment_;
  MockObserver observer_;
  ApplicationLocaleStorage locale_storage_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  raw_ptr<MockNtpBackgroundService> mock_background_service_;
  std::unique_ptr<NtpAndroidCustomBackgroundService> service_;
};

TEST_F(NtpAndroidCustomBackgroundServiceTest, RegisterAllPrefs) {
  EXPECT_TRUE(profile_->GetPrefs()->FindPreference(
      prefs::kNtpAndroidCustomBackgroundDict));
  EXPECT_TRUE(profile_->GetPrefs()->FindPreference(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest, SetCustomBackgroundInfo) {
  GURL bg_url(kTestBackgroundUrl);
  GURL thumb_url(kTestThumbnailUrl);
  std::string attr1 = kTestAttribution1;
  std::string attr2 = kTestAttribution2;
  GURL action_url(kTestActionUrl);
  std::string collection_id = kTestCollectionId;

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);
  mock_background_service_->AddValidBackdropUrlForTesting(bg_url);

  service_->SetCustomBackgroundInfo(bg_url, thumb_url, attr1, attr2, action_url,
                                    collection_id);

  std::optional<CustomBackground> bg = service_->GetCustomBackground();
  ASSERT_TRUE(bg.has_value());
  EXPECT_EQ(bg->custom_background_url, bg_url);
  EXPECT_EQ(bg->collection_id, collection_id);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SetCustomBackgroundURLInvalidURL) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(2);

  const GURL kInvalidUrl(kTestInvalidUrl);
  const GURL kValidUrl(kTestValidUrl);

  mock_background_service_->AddValidBackdropUrlForTesting(kValidUrl);

  service_->SetCustomBackgroundInfo(kValidUrl, GURL(), "", "", GURL(), "");
  EXPECT_TRUE(service_->GetCustomBackground().has_value());

  service_->SetCustomBackgroundInfo(kInvalidUrl, GURL(), "", "", GURL(), "");
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest, UpdatingPrefUpdatesNtpTheme) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);

  const GURL kUrl(kTestPrefUrl);
  base::DictValue background_info;
  background_info.Set(kNtpCustomBackgroundURL, kUrl.spec());
  background_info.Set(kNtpCustomBackgroundCollectionId, kTestSomeId);
  background_info.Set(kNtpCustomBackgroundRefreshTimestamp, 1);

  profile_->GetPrefs()->SetDict(prefs::kNtpAndroidCustomBackgroundDict,
                                std::move(background_info));

  std::optional<CustomBackground> bg = service_->GetCustomBackground();
  ASSERT_TRUE(bg.has_value());
  EXPECT_EQ(bg->custom_background_url, kUrl);
  EXPECT_EQ(bg->collection_id, kTestSomeId);
  EXPECT_TRUE(bg->daily_refresh_enabled);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest, ResetCustomBackgroundInfo) {
  GURL bg_url(kTestBackgroundUrl);
  mock_background_service_->AddValidBackdropUrlForTesting(bg_url);
  service_->SetCustomBackgroundInfo(bg_url, GURL(), "", "", GURL(), "");

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);
  service_->ResetCustomBackgroundInfo();

  std::optional<CustomBackground> bg = service_->GetCustomBackground();
  EXPECT_FALSE(bg.has_value());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest, SelectLocalBackgroundImage) {
  service_->SelectLocalBackgroundImage(base::FilePath());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest, UpdateBackgroundFromSync) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice, true);

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);
  service_->UpdateBackgroundFromSync();

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SetCustomBackgroundInfo_BackdropCollection) {
  std::string collection_id = kTestBackdropCollectionId;
  EXPECT_CALL(*mock_background_service_,
              IsValidBackdropCollection(collection_id))
      .WillOnce(testing::Return(true));

  service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "", GURL(),
                                    collection_id);

  GURL expected_url = mock_background_service_->GetNextImageURLForTesting();
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_url.spec()));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SetCustomBackgroundInfo_ForcedRefresh) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice, true);
  profile_->GetPrefs()->ClearPref(prefs::kNtpAndroidCustomBackgroundDict);

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);
  service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "", GURL(), "");

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       RefreshBackgroundIfNeeded_EmptyPrefs) {
  profile_->GetPrefs()->ClearPref(prefs::kNtpAndroidCustomBackgroundDict);
  // This should safely early-return and NOT crash.
  service_->RefreshBackgroundIfNeeded();
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       NotifyAboutBackgrounds_RoutesToThemeCollectionBridge_OnStaticImage) {
  MockThemeCollectionBridge mock_theme_bridge;
  MockSyncedThemeBridge mock_synced_bridge;
  service_->SetThemeCollectionBridge(&mock_theme_bridge);
  service_->SetSyncedThemeBridge(&mock_synced_bridge);

  EXPECT_CALL(mock_theme_bridge, OnCustomBackgroundImageUpdated()).Times(1);
  EXPECT_CALL(mock_synced_bridge, OnCustomBackgroundImageUpdated()).Times(0);

  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  service_->SetThemeCollectionBridge(nullptr);
  service_->SetSyncedThemeBridge(nullptr);
}

TEST_F(
    NtpAndroidCustomBackgroundServiceTest,
    NotifyAboutBackgrounds_RoutesToThemeCollectionBridge_OnInitialDailyRefreshSetup) {
  MockThemeCollectionBridge mock_theme_bridge;
  MockSyncedThemeBridge mock_synced_bridge;
  service_->SetThemeCollectionBridge(&mock_theme_bridge);
  service_->SetSyncedThemeBridge(&mock_synced_bridge);

  EXPECT_CALL(mock_theme_bridge, OnCustomBackgroundImageUpdated()).Times(1);
  EXPECT_CALL(mock_synced_bridge, OnCustomBackgroundImageUpdated()).Times(0);

  // Initial setup sets an empty URL for the collection.
  service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "", GURL(),
                                    kTestCollectionId);

  // Simulate arrival of first collection image. Since initial URL was empty,
  // IsNextThemeCollectionImage returns false, routing to
  // theme_collection_bridge_.
  base::DictValue background_info;
  background_info.Set(kNtpCustomBackgroundURL, kTestValidUrl);
  background_info.Set(kNtpCustomBackgroundCollectionId, kTestCollectionId);
  background_info.Set(kNtpCustomBackgroundRefreshTimestamp, 1);
  profile_->GetPrefs()->SetDict(prefs::kNtpAndroidCustomBackgroundDict,
                                std::move(background_info));

  service_->SetThemeCollectionBridge(nullptr);
  service_->SetSyncedThemeBridge(nullptr);
}

TEST_F(
    NtpAndroidCustomBackgroundServiceTest,
    NotifyAboutBackgrounds_RoutesToSyncedThemeBridge_OnNextDailyRefreshCycle) {
  MockThemeCollectionBridge mock_theme_bridge;
  MockSyncedThemeBridge mock_synced_bridge;
  service_->SetThemeCollectionBridge(&mock_theme_bridge);
  service_->SetSyncedThemeBridge(&mock_synced_bridge);

  // Setup initial active daily refresh with an empty URL first.
  service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  base::DictValue initial_info;
  initial_info.Set(kNtpCustomBackgroundURL, kTestValidUrl);
  initial_info.Set(kNtpCustomBackgroundCollectionId, kTestCollectionId);
  initial_info.Set(kNtpCustomBackgroundRefreshTimestamp, 1);
  profile_->GetPrefs()->SetDict(prefs::kNtpAndroidCustomBackgroundDict,
                                std::move(initial_info));

  testing::Mock::VerifyAndClearExpectations(&mock_theme_bridge);
  testing::Mock::VerifyAndClearExpectations(&mock_synced_bridge);

  EXPECT_CALL(mock_theme_bridge, OnCustomBackgroundImageUpdated()).Times(0);
  EXPECT_CALL(mock_synced_bridge, OnCustomBackgroundImageUpdated()).Times(1);

  // Simulate subsequent daily refresh cycle image arrival.
  base::DictValue background_info;
  background_info.Set(kNtpCustomBackgroundURL, kTestValidUrl2);
  background_info.Set(kNtpCustomBackgroundCollectionId, kTestCollectionId);
  background_info.Set(kNtpCustomBackgroundRefreshTimestamp, 2);
  profile_->GetPrefs()->SetDict(prefs::kNtpAndroidCustomBackgroundDict,
                                std::move(background_info));

  service_->SetThemeCollectionBridge(nullptr);
  service_->SetSyncedThemeBridge(nullptr);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       NotifyAboutBackgrounds_NullBridgesSafety) {
  service_->SetThemeCollectionBridge(nullptr);
  service_->SetSyncedThemeBridge(nullptr);
  // Should safely execute without crashing.
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       OnNextCollectionImageAvailable_IgnoredWhenNoActiveBackground) {
  service_->ResetCustomBackgroundInfo();
  service_->OnNextCollectionImageAvailable();
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       OnNextCollectionImageAvailable_IgnoredWhenDailyRefreshDisabled) {
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  CollectionImage next_img;
  next_img.collection_id = kTestCollectionId;
  next_img.image_url = GURL(kTestValidUrl2);
  mock_background_service_->SetNextCollectionImageForTesting(next_img);

  service_->OnNextCollectionImageAvailable();
  std::optional<CustomBackground> bg = service_->GetCustomBackground();
  ASSERT_TRUE(bg.has_value());
  EXPECT_EQ(bg->custom_background_url, GURL(kTestValidUrl));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       OnNextCollectionImageAvailable_IgnoredWhenCollectionIdMismatch) {
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  CollectionImage next_img;
  next_img.collection_id = kTestCollectionIdA;
  next_img.image_url = GURL(kTestValidUrl2);
  mock_background_service_->SetNextCollectionImageForTesting(next_img);

  service_->OnNextCollectionImageAvailable();
  std::optional<CustomBackground> bg = service_->GetCustomBackground();
  ASSERT_TRUE(bg.has_value());
  EXPECT_EQ(bg->collection_id, kTestCollectionId);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       ActiveCustomBackground_StateTransitionsAndReset) {
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  EXPECT_TRUE(service_->GetCustomBackground().has_value());

  service_->SelectLocalBackgroundImage(base::FilePath());
  // Local selection should clear theme collection background.
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest, GetNextRefreshTimestamp) {
  EXPECT_EQ(INT_MAX, service_->GetNextRefreshTimestamp());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       OnThemeChangedFromSync_WithValidBackgroundSpecifics) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestValidUrl);
  specifics.mutable_ntp_background()->set_collection_id(kTestCollectionId);
  specifics.mutable_ntp_background()->set_attribution_line_1(kTestAttribution1);
  specifics.mutable_ntp_background()->set_attribution_line_2(kTestAttribution2);
  specifics.mutable_ntp_background()->set_attribution_action_url(
      kTestActionUrl);

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);

  service_->OnThemeChangedFromSync(specifics);

  EXPECT_TRUE(service_->IsProcessingSyncUpdate());
  std::optional<CustomBackground> bg = service_->GetCustomBackground();
  ASSERT_TRUE(bg.has_value());
  EXPECT_EQ(bg->custom_background_url, GURL(kTestValidUrl));
  EXPECT_EQ(bg->collection_id, kTestCollectionId);
  EXPECT_EQ(bg->custom_background_attribution_line_1, kTestAttribution1);
  EXPECT_EQ(bg->custom_background_attribution_line_2, kTestAttribution2);
  EXPECT_EQ(bg->custom_background_attribution_action_url, GURL(kTestActionUrl));
  EXPECT_FALSE(bg->daily_refresh_enabled);

  const base::Value* pref = profile_->GetPrefs()->GetUserPrefValue(
      prefs::kNtpAndroidCustomBackgroundDict);
  ASSERT_TRUE(pref != nullptr && pref->is_dict());
  EXPECT_EQ(kTestValidUrl,
            *pref->GetDict().FindString(kNtpCustomBackgroundURL));
  EXPECT_EQ(kTestCollectionId,
            *pref->GetDict().FindString(kNtpCustomBackgroundCollectionId));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       OnThemeChangedFromSync_WithEmptyBackgroundSpecifics_ClearsPref) {
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  EXPECT_TRUE(service_->GetCustomBackground().has_value());

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated()).Times(1);

  sync_pb::ThemeAndroidSpecifics empty_specifics;
  service_->OnThemeChangedFromSync(empty_specifics);

  EXPECT_TRUE(service_->IsProcessingSyncUpdate());
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
  const base::Value* pref = profile_->GetPrefs()->GetUserPrefValue(
      prefs::kNtpAndroidCustomBackgroundDict);
  EXPECT_TRUE(pref == nullptr || pref->GetDict().empty());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       NotifyAboutBackgrounds_RoutesToSyncedThemeBridge_OnSyncUpdate) {
  MockThemeCollectionBridge mock_theme_bridge;
  MockSyncedThemeBridge mock_synced_bridge;
  service_->SetThemeCollectionBridge(&mock_theme_bridge);
  service_->SetSyncedThemeBridge(&mock_synced_bridge);

  EXPECT_CALL(mock_theme_bridge, OnCustomBackgroundImageUpdated()).Times(0);
  EXPECT_CALL(mock_synced_bridge, OnCustomBackgroundImageUpdated()).Times(1);

  service_->OnThemeChangedFromSync(CreateTestThemeSpecifics());

  service_->SetThemeCollectionBridge(nullptr);
  service_->SetSyncedThemeBridge(nullptr);
}

TEST_F(
    NtpAndroidCustomBackgroundServiceTest,
    SetSyncedThemeBridge_NotifiesBridge_WhenExistingSyncedBackgroundPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kNewTabPageCustomizationThemeSync);

  // Simulate an existing synced background already saved in preferences.
  service_->OnThemeChangedFromSync(CreateTestThemeSpecifics());

  MockSyncedThemeBridge mock_synced_bridge;
  EXPECT_CALL(mock_synced_bridge, OnCustomBackgroundImageUpdated()).Times(1);

  // Attaching the bridge should asynchronously notify it of the existing synced
  // theme.
  service_->SetSyncedThemeBridge(&mock_synced_bridge);
  task_environment_.RunUntilIdle();

  service_->SetSyncedThemeBridge(nullptr);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SetSyncedThemeBridge_DoesNotNotifyBridge_WhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kNewTabPageCustomizationThemeSync);

  // Simulate an existing synced background already saved in preferences.
  service_->OnThemeChangedFromSync(CreateTestThemeSpecifics());

  MockSyncedThemeBridge mock_synced_bridge;
  EXPECT_CALL(mock_synced_bridge, OnCustomBackgroundImageUpdated()).Times(0);

  service_->SetSyncedThemeBridge(&mock_synced_bridge);
  task_environment_.RunUntilIdle();

  service_->SetSyncedThemeBridge(nullptr);
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SyncBridgeIntegration_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kNewTabPageCustomizationThemeSync);

  auto service = std::make_unique<NtpAndroidCustomBackgroundService>(
      profile_.get(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
  EXPECT_EQ(nullptr, service->GetSyncControllerDelegate());
  EXPECT_FALSE(service->IsProcessingSyncUpdate());

  service->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                   kTestCollectionId);
  service->SelectLocalBackgroundImage(base::FilePath());
  service->ResetCustomBackgroundInfo();
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SyncBridgeIntegration_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kNewTabPageCustomizationThemeSync);

  auto service = std::make_unique<NtpAndroidCustomBackgroundService>(
      profile_.get(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
  EXPECT_NE(nullptr, service->GetSyncControllerDelegate());
  EXPECT_FALSE(service->IsProcessingSyncUpdate());
}

TEST_F(
    NtpAndroidCustomBackgroundServiceTest,
    SelectLocalBackgroundImage_ResetsProcessingSyncUpdateAndSetsLocalToDevice) {
  service_->OnThemeChangedFromSync(CreateTestThemeSpecifics());
  EXPECT_TRUE(service_->IsProcessingSyncUpdate());

  service_->SelectLocalBackgroundImage(base::FilePath());
  EXPECT_FALSE(service_->IsProcessingSyncUpdate());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
}

TEST_F(
    NtpAndroidCustomBackgroundServiceTest,
    ResetCustomBackgroundInfo_ResetsProcessingSyncUpdateAndClearsBackground) {
  service_->OnThemeChangedFromSync(CreateTestThemeSpecifics());
  EXPECT_TRUE(service_->IsProcessingSyncUpdate());

  service_->ResetCustomBackgroundInfo();
  EXPECT_FALSE(service_->IsProcessingSyncUpdate());
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       SetCustomBackgroundInfo_ResetsProcessingSyncUpdate) {
  service_->OnThemeChangedFromSync(CreateTestThemeSpecifics());
  EXPECT_TRUE(service_->IsProcessingSyncUpdate());

  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  EXPECT_FALSE(service_->IsProcessingSyncUpdate());
  EXPECT_TRUE(service_->GetCustomBackground().has_value());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       UpdateCustomBackgroundPrefsWithColor_UpdatesPrefAndSyncBridge) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kNewTabPageCustomizationThemeSync);

  auto service = std::make_unique<NtpAndroidCustomBackgroundService>(
      profile_.get(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));

  service->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                   kTestCollectionId);

  constexpr SkColor kTestColor = SK_ColorRED;
  EXPECT_TRUE(service->UpdateCustomBackgroundPrefsWithColor(GURL(kTestValidUrl),
                                                            kTestColor));

  const base::Value* pref = profile_->GetPrefs()->GetUserPrefValue(
      prefs::kNtpAndroidCustomBackgroundDict);
  ASSERT_TRUE(pref != nullptr && pref->is_dict());
  EXPECT_EQ(static_cast<int>(kTestColor),
            pref->GetDict().FindInt(kNtpCustomBackgroundMainColor));

  std::map<std::string, sync_pb::ThemeAndroidSpecifics> specifics_map =
      syncer::DataTypeStoreTestUtil::ReadAllDataAsProtoAndWait<
          sync_pb::ThemeAndroidSpecifics>(*store_);
  ASSERT_EQ(1u, specifics_map.size());
  const sync_pb::ThemeAndroidSpecifics& specifics =
      specifics_map[kAndroidThemeStorageKey];
  EXPECT_TRUE(specifics.has_ntp_background());
  EXPECT_EQ(kTestValidUrl, specifics.ntp_background().url());
  EXPECT_EQ(kTestCollectionId, specifics.ntp_background().collection_id());
  EXPECT_EQ(kTestColor, specifics.ntp_background().main_color());
  EXPECT_TRUE(specifics.has_user_color_theme());
  EXPECT_EQ(kTestColor, specifics.user_color_theme().color());
  EXPECT_EQ(sync_pb::UserColorTheme::TONAL_SPOT,
            specifics.user_color_theme().browser_color_variant());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       UpdateCustomBackgroundPrefsWithColor_MismatchedUrl_DoesNothing) {
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);

  constexpr SkColor kTestColor = SK_ColorRED;
  EXPECT_FALSE(service_->UpdateCustomBackgroundPrefsWithColor(
      GURL(kTestValidUrl2), kTestColor));

  const base::Value* pref = profile_->GetPrefs()->GetUserPrefValue(
      prefs::kNtpAndroidCustomBackgroundDict);
  ASSERT_TRUE(pref != nullptr && pref->is_dict());
  EXPECT_FALSE(
      pref->GetDict().FindInt(kNtpCustomBackgroundMainColor).has_value());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       UpdateCustomBackgroundPrefsWithColor_EmptyPref_DoesNothing) {
  constexpr SkColor kTestColor = SK_ColorBLUE;
  EXPECT_FALSE(service_->UpdateCustomBackgroundPrefsWithColor(
      GURL(kTestValidUrl), kTestColor));

  EXPECT_FALSE(service_->GetCustomBackground().has_value());
  const base::Value* pref = profile_->GetPrefs()->GetUserPrefValue(
      prefs::kNtpAndroidCustomBackgroundDict);
  EXPECT_TRUE(pref == nullptr || pref->GetDict().empty());
}

TEST_F(NtpAndroidCustomBackgroundServiceTest,
       UpdateCustomBackgroundPrefsWithColor_DoesNotNotifyObserversOrBridge) {
  MockThemeCollectionBridge bridge;
  MockObserver observer;
  service_->SetThemeCollectionBridge(&bridge);
  service_->AddObserver(&observer);

  // Initial background selection notifies both the bridge and observers.
  EXPECT_CALL(bridge, OnCustomBackgroundImageUpdated).Times(1);
  EXPECT_CALL(observer, OnCustomBackgroundImageUpdated).Times(1);
  service_->SetCustomBackgroundInfo(GURL(kTestValidUrl), GURL(), "", "", GURL(),
                                    kTestCollectionId);
  testing::Mock::VerifyAndClearExpectations(&bridge);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Updating the primary color suppresses re-entrant notifications so observers
  // and the bridge should NOT be called.
  EXPECT_CALL(bridge, OnCustomBackgroundImageUpdated).Times(0);
  EXPECT_CALL(observer, OnCustomBackgroundImageUpdated).Times(0);
  constexpr SkColor kTestColor = SK_ColorRED;
  EXPECT_TRUE(service_->UpdateCustomBackgroundPrefsWithColor(
      GURL(kTestValidUrl), kTestColor));

  service_->RemoveObserver(&observer);
  service_->SetThemeCollectionBridge(nullptr);
}

}  // namespace
