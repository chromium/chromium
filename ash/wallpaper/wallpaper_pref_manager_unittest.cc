// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_pref_manager.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using testing::AllOf;
using testing::Gt;
using testing::Lt;

constexpr char kUser1[] = "user1@test.com";
const AccountId account_id_1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

constexpr char kDummyUrl[] = "https://best_wallpaper/1";
constexpr char kDummyUrl2[] = "https://best_wallpaper/2";
constexpr char kDummyUrl3[] = "https://best_wallpaper/3";
constexpr char kDummyUrl4[] = "https://best_wallpaper/4";

const uint64_t kAssetId = 1;
const uint64_t kAssetId2 = 2;
const uint64_t kAssetId3 = 3;
const uint64_t kAssetId4 = 4;

constexpr char kFakeGooglePhotosPhotoId[] = "fake_photo";

WallpaperInfo InfoWithType(WallpaperType type) {
  return WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, type,
                       base::Time::Now());
}

void PutWallpaperInfoInPrefs(AccountId account_id,
                             WallpaperInfo info,
                             PrefService* pref_service,
                             const std::string& pref_name) {
  DCHECK(pref_service);
  ScopedDictPrefUpdate wallpaper_update(pref_service, pref_name);
  wallpaper_update->Set(account_id.GetUserEmail(), info.ToDict());
}

void AssertWallpaperInfoInPrefs(const PrefService* pref_service,
                                const char pref_name[],
                                AccountId account_id,
                                const WallpaperInfo& info) {
  const base::Value::Dict* stored_info_dict =
      pref_service->GetDict(pref_name).FindDict(account_id.GetUserEmail());
  DCHECK(stored_info_dict);
  EXPECT_EQ(info.ToDict(), *stored_info_dict);
}

std::string GetDummyFileName(const AccountId& account_id) {
  return account_id.GetUserEmail() + "-file";
}

class TestProfileHelper : public WallpaperProfileHelper {
 public:
  TestProfileHelper() = default;

  // Create a PrefService for |account_id| if it doesn't exist and register the
  // preference keys.
  void RegisterPrefsForAccount(const AccountId& account_id) {
    TestingPrefServiceSimple* service = &synced_prefs_[account_id];
    WallpaperPrefManager::RegisterProfilePrefs(service->registry());
  }

  void SetClient(WallpaperControllerClient*) override {}

  PrefService* GetUserPrefServiceSyncable(const AccountId& id) override {
    if (!is_sync_enabled) {
      return nullptr;
    }

    const auto& pref = synced_prefs_.find(id);
    return pref == synced_prefs_.end() ? nullptr : &(pref->second);
  }

  bool IsActiveUserSessionStarted() const override {
    return is_session_started;
  }

  AccountId GetActiveAccountId() const override { return active_account; }

  bool IsWallpaperSyncEnabled(const AccountId&) const override {
    return is_sync_enabled;
  }

  bool IsEphemeral(const AccountId&) const override { return is_ephemeral; }

  bool is_ephemeral = false;
  bool is_session_started = true;
  bool is_sync_enabled = true;
  AccountId active_account;

 private:
  std::map<AccountId, TestingPrefServiceSimple> synced_prefs_;
};

class WallpaperPrefManagerTestBase : public testing::Test {
 public:
  WallpaperPrefManagerTestBase() = default;

  WallpaperPrefManagerTestBase(const WallpaperPrefManagerTestBase&) = delete;
  WallpaperPrefManagerTestBase& operator=(const WallpaperPrefManagerTestBase&) =
      delete;

  ~WallpaperPrefManagerTestBase() override = default;

  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    WallpaperPrefManager::RegisterLocalStatePrefs(local_state_->registry());

    auto profile_helper = std::make_unique<TestProfileHelper>();
    profile_helper_ = profile_helper.get();
    pref_manager_ = WallpaperPrefManager::CreateForTesting(
        local_state_.get(), std::move(profile_helper));
  }

  PrefService* GetLocalPrefService() { return local_state_.get(); }

  void SimulateUserLogin(const AccountId& id) {
    profile_helper_->RegisterPrefsForAccount(id);
  }

  void StoreWallpaper(const AccountId& account_id, std::string_view location) {
    WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
    info.location = std::string(location);
    ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id, info));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  raw_ptr<TestProfileHelper, DanglingUntriaged> profile_helper_;

  TestWallpaperControllerClient client_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;

  std::unique_ptr<WallpaperPrefManager> pref_manager_;
};

class WallpaperPrefManagerTest : public WallpaperPrefManagerTestBase {
 public:
  WallpaperPrefManagerTest() = default;
};

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfo_Normal) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, expected_info);

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(account_id_1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(expected_info));
}

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfo_Ephemeral) {
  profile_helper_->is_ephemeral = true;
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, expected_info);

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(account_id_1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(expected_info));
}

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfoNothingToGet_Normal) {
  WallpaperInfo info;
  EXPECT_FALSE(pref_manager_->GetUserWallpaperInfo(account_id_1, &info));
}

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfoNothingToGet_Ephemeral) {
  profile_helper_->is_ephemeral = true;
  WallpaperInfo info;
  EXPECT_FALSE(pref_manager_->GetUserWallpaperInfo(account_id_1, &info));
}

TEST_F(WallpaperPrefManagerTest,
       GetWallpaperInfo_FromEphemeralForManagedGuestSessions) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kPolicy);
  pref_manager_->SetUserWallpaperInfo(account_id_1, /*is_ephemeral=*/true,
                                      expected_info);

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(
      account_id_1, /*is_ephemeral=*/true, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(expected_info));
}

TEST_F(WallpaperPrefManagerTest, RemoveUserWallpaperInfo_Ephemeral) {
  profile_helper_->is_ephemeral = true;
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, expected_info);

  pref_manager_->RemoveUserWallpaperInfo(account_id_1);
  WallpaperInfo actual_info;
  EXPECT_FALSE(pref_manager_->GetUserWallpaperInfo(account_id_1, &actual_info));
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfo_EphemeralDoesNotChangeLocal) {
  profile_helper_->is_ephemeral = true;
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, expected_info);

  // Local state is expected to be untouched for ephemeral users.
  EXPECT_EQ(nullptr, local_state_->GetUserPrefValue(prefs::kUserWallpaperInfo));
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoLocal) {
  WallpaperInfo info(
      GetDummyFileName(account_id_1), WALLPAPER_LAYOUT_CENTER_CROPPED,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));
  AssertWallpaperInfoInPrefs(local_state_.get(), prefs::kUserWallpaperInfo,
                             account_id_1, info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoLocalFromGooglePhotos) {
  WallpaperInfo info(
      GooglePhotosWallpaperParams{account_id_1, kFakeGooglePhotosPhotoId,
                                  /*daily_refresh_enabled=*/false,
                                  WallpaperLayout::WALLPAPER_LAYOUT_STRETCH,
                                  /*preview_mode=*/false, "dedup_key"});
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));
  AssertWallpaperInfoInPrefs(GetLocalPrefService(), prefs::kUserWallpaperInfo,
                             account_id_1, info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoSynced) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo info = InfoWithType(WallpaperType::kOnline);
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));
  AssertWallpaperInfoInPrefs(
      profile_helper_->GetUserPrefServiceSyncable(account_id_1),
      prefs::kSyncableWallpaperInfo, account_id_1, info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoSyncedFromGooglePhotos) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo info = InfoWithType(WallpaperType::kOnceGooglePhotos);
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));
  AssertWallpaperInfoInPrefs(
      profile_helper_->GetUserPrefServiceSyncable(account_id_1),
      prefs::kSyncableWallpaperInfo, account_id_1, info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoSyncDisabled) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);
  // This needs to be saved before sync is disabled or we can't get a pref
  // service.
  PrefService* syncable_prefs =
      profile_helper_->GetUserPrefServiceSyncable(account_id_1);
  profile_helper_->is_sync_enabled = false;

  WallpaperInfo expected_info = InfoWithType(WallpaperType::kCustomized);
  PutWallpaperInfoInPrefs(account_id_1, expected_info, syncable_prefs,
                          prefs::kSyncableWallpaperInfo);

  WallpaperInfo info = InfoWithType(WallpaperType::kOnline);
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));

  // Verify that calling SetUserWallpaperInfo does NOT change what is in synced
  // prefs when sync is disabled.
  AssertWallpaperInfoInPrefs(syncable_prefs, prefs::kSyncableWallpaperInfo,
                             account_id_1, expected_info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoCustom) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnline);
  PutWallpaperInfoInPrefs(
      account_id_1, synced_info,
      profile_helper_->GetUserPrefServiceSyncable(account_id_1),
      prefs::kSyncableWallpaperInfo);

  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));

  // Custom wallpaper infos should not be propagated to synced preferences until
  // the image is uploaded to drivefs. That is not done in
  // |SetUserWallpaperInfo|.
  AssertWallpaperInfoInPrefs(
      profile_helper_->GetUserPrefServiceSyncable(account_id_1),
      prefs::kSyncableWallpaperInfo, account_id_1, synced_info);
}

TEST_F(WallpaperPrefManagerTest, GetNextDailyRefreshUpdate_Future) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);
  base::Time time = base::Time::Now();

  WallpaperInfo info = InfoWithType(WallpaperType::kDaily);
  info.date = time + base::Days(2);

  ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));

  // Next update should be date + 1 day.
  EXPECT_THAT(pref_manager_->GetTimeToNextDailyRefreshUpdate(account_id_1),
              AllOf(Gt(base::Days(3) - base::Minutes(1)),
                    Lt(base::Days(3) + base::Minutes(1))));
}

TEST_F(WallpaperPrefManagerTest, GetNextDailyRefreshUpdate_Past) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);
  base::Time time = base::Time::Now();

  WallpaperInfo info = InfoWithType(WallpaperType::kDaily);
  info.date = time - base::Days(2);

  ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));

  // Next update should be immediate if it would be negative.
  EXPECT_EQ(pref_manager_->GetTimeToNextDailyRefreshUpdate(account_id_1),
            base::TimeDelta());
}

TEST_F(WallpaperPrefManagerTest, GetNextDailyRefreshUpdate_Recent) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);
  base::Time time = base::Time::Now();

  WallpaperInfo info = InfoWithType(WallpaperType::kDaily);
  info.date = time - base::Hours(2);

  ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, info));

  // Next update should be 24 hours +- 1 minute after the date on WallpaperInfo.
  EXPECT_THAT(pref_manager_->GetTimeToNextDailyRefreshUpdate(account_id_1),
              AllOf(Gt(base::Hours(22) - base::Minutes(1)),
                    Lt(base::Hours(22) + base::Minutes(1))));
}

TEST_F(WallpaperPrefManagerTest, CacheKMeansColor) {
  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  const char location[] = "/test/location";
  info.location = location;

  const SkColor expected_color = SkColorSetRGB(0xAB, 0xBC, 0xEF);
  pref_manager_->CacheKMeanColor(location, expected_color);
  EXPECT_EQ(expected_color, *pref_manager_->GetCachedKMeanColor(location));
}

TEST_F(WallpaperPrefManagerTest, RemoveKMeansColor) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);
  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  const char location[] = "/test/location";
  info.location = location;

  StoreWallpaper(account_id_1, location);

  pref_manager_->CacheKMeanColor(location, SkColorSetRGB(0xFF, 0xFF, 0xFF));
  pref_manager_->RemoveKMeanColor(account_id_1);
  EXPECT_FALSE(pref_manager_->GetCachedKMeanColor(location));
}

TEST_F(WallpaperPrefManagerTest, CacheCelebiColor) {
  const char location[] = "/test/location";

  const SkColor expected_color = SkColorSetRGB(0xAB, 0xBC, 0xEF);
  pref_manager_->CacheCelebiColor(location, expected_color);

  std::optional<SkColor> color = pref_manager_->GetCelebiColor(location);
  ASSERT_TRUE(color);
  EXPECT_EQ(expected_color, *color);
}

TEST_F(WallpaperPrefManagerTest, RemoveCelebiColor) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);
  const char location[] = "/test/location";

  pref_manager_->CacheCelebiColor(location, SkColorSetRGB(0xFF, 0xFF, 0xFF));

  StoreWallpaper(account_id_1, location);

  pref_manager_->RemoveCelebiColor(account_id_1);
  EXPECT_FALSE(pref_manager_->GetCelebiColor(location));
}

TEST_F(WallpaperPrefManagerTest, CalculatedColors) {
  const char location[] = "location";

  const SkColor k_mean_color = SkColorSetRGB(0xAB, 0xBC, 0xEF);
  pref_manager_->CacheKMeanColor(location, k_mean_color);

  const SkColor celebi_color = SkColorSetRGB(0xFF, 0xCC, 0x22);
  pref_manager_->CacheCelebiColor(location, celebi_color);

  std::optional<WallpaperCalculatedColors> actual_colors =
      pref_manager_->GetCachedWallpaperColors(location);
  ASSERT_TRUE(actual_colors);
  EXPECT_EQ(k_mean_color, actual_colors->k_mean_color);
  EXPECT_EQ(celebi_color, actual_colors->celebi_color);
}

TEST_F(WallpaperPrefManagerTest, CalculatedColorsEmptyIfKMeanMissing) {
  const char location[] = "location";

  const SkColor celebi_color = SkColorSetRGB(0xFF, 0xCC, 0x22);
  pref_manager_->CacheCelebiColor(location, celebi_color);

  EXPECT_FALSE(pref_manager_->GetCachedWallpaperColors(location));
}

TEST_F(WallpaperPrefManagerTest, CalculatedColorsEmptyIfCelebiMissing) {
  const char location[] = "location";

  const SkColor k_mean_color = SkColorSetRGB(0xAB, 0xBC, 0xEF);
  pref_manager_->CacheKMeanColor(location, k_mean_color);

  EXPECT_FALSE(pref_manager_->GetCachedWallpaperColors(location));
}

TEST_F(WallpaperPrefManagerTest, ShouldSyncOut) {
  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncOut(
      InfoWithType(WallpaperType::kOnline)));

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId3, GURL(kDummyUrl3),
                        backdrop::Image::IMAGE_TYPE_MORNING_MODE);
  variants.emplace_back(kAssetId4, GURL(kDummyUrl4),
                        backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE);
  WallpaperInfo info = InfoWithType(WallpaperType::kOnline);
  info.variants = variants;
  info.collection_id = wallpaper_constants::kTimeOfDayWallpaperCollectionId;
  EXPECT_FALSE(WallpaperPrefManager::ShouldSyncOut(info));
}

TEST_F(WallpaperPrefManagerTest, ShouldSyncIn) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kDaily);
  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                 /*is_oobe=*/false));

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId3, GURL(kDummyUrl3),
                        backdrop::Image::IMAGE_TYPE_MORNING_MODE);
  variants.emplace_back(kAssetId4, GURL(kDummyUrl4),
                        backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE);
  local_info.variants = variants;
  local_info.collection_id =
      wallpaper_constants::kTimeOfDayWallpaperCollectionId;
  EXPECT_FALSE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                  /*is_oobe=*/false));
  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                 /*is_oobe=*/true));
}

TEST_F(WallpaperPrefManagerTest, ShouldNotSyncInIfLocalWallpaperIsSeaPen) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kSeaPen);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kDaily);
  local_info.location = "6868";
  EXPECT_FALSE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                  /*is_oobe=*/false));
}

TEST_F(WallpaperPrefManagerTest,
       SetUserWallpaperInfoChecksVersionWhenVersionedWallpaperInfoIsEnabled) {
  base::test::ScopedFeatureList features(features::kVersionedWallpaperInfo);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);

  EXPECT_TRUE(local_info.version.IsValid());
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, local_info));
}

TEST_F(WallpaperPrefManagerTest, ShouldNotSyncInIfSyncPrefHasInvalidVersion) {
  base::test::ScopedFeatureList features(features::kVersionedWallpaperInfo);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnline);
  synced_info.version = base::Version();

  EXPECT_FALSE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                  /*is_oobe=*/false));
}

TEST_F(WallpaperPrefManagerTest,
       ShouldNotSyncInIfSyncPrefHasUnsupportedVersion) {
  base::test::ScopedFeatureList features(features::kVersionedWallpaperInfo);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnline);
  synced_info.version = base::Version("99.99");

  EXPECT_FALSE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                  /*is_oobe=*/false));
}

TEST_F(WallpaperPrefManagerTest,
       ShouldSyncInIfSyncPrefHasSameVersionAsSupportedVersion) {
  base::test::ScopedFeatureList features(features::kVersionedWallpaperInfo);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kDaily);
  synced_info.version = base::Version("1.0");

  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                 /*is_oobe=*/false));
}

TEST_F(WallpaperPrefManagerTest,
       ShouldSyncInIfSyncPrefHasSameMajorButDifferentMinorVersion) {
  base::test::ScopedFeatureList features(features::kVersionedWallpaperInfo);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnline);
  synced_info.version = base::Version("1.1");

  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                 /*is_oobe=*/false));
}

TEST_F(WallpaperPrefManagerTest,
       ShouldSyncInIfSyncPrefHasOlderVersionThanSupportedVersion) {
  base::test::ScopedFeatureList features(features::kVersionedWallpaperInfo);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnline);
  synced_info.version = base::Version("0.1");

  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                 /*is_oobe=*/false));
}

TEST_F(WallpaperPrefManagerTest,
       VersionIsIgnoredWhenVersionWallpaperInfoIsDisabled) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  local_info.version = base::Version("1.0");
  WallpaperInfo synced_info = InfoWithType(WallpaperType::kDaily);
  synced_info.version = base::Version("2.0");

  EXPECT_TRUE(WallpaperPrefManager::ShouldSyncIn(synced_info, local_info,
                                                 /*is_oobe=*/false));
}

// Verifies that creating a wallpaper info from prefs with an invalid layout
// enum fails.
TEST_F(WallpaperPrefManagerTest, GetSyncedWallpaperInfo_InvalidLayoutEnum) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  base::Value::Dict wallpaper_info_dict = info.ToDict();

  // Mangles pref data with invalid layout.
  int invalid_layout = 1000;
  wallpaper_info_dict.Set(WallpaperInfo::kNewWallpaperLayoutNodeName,
                          invalid_layout);

  PrefService* syncable_prefs =
      profile_helper_->GetUserPrefServiceSyncable(account_id_1);
  DCHECK(syncable_prefs);
  ScopedDictPrefUpdate wallpaper_update(syncable_prefs,
                                        prefs::kSyncableWallpaperInfo);
  wallpaper_update->Set(account_id_1.GetUserEmail(),
                        std::move(wallpaper_info_dict));
  WallpaperInfo actual_info;
  EXPECT_FALSE(
      pref_manager_->GetSyncedWallpaperInfo(account_id_1, &actual_info));
}

// Verifies that creating a wallpaper info from prefs with an invalid wallpaper
// type fails.
TEST_F(WallpaperPrefManagerTest, GetSyncedWallpaperInfo_InvalidWallpaperType) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  base::Value::Dict wallpaper_info_dict = info.ToDict();

  // Mangles pref data with invalid wallpaper type.
  int invalid_wallpaper_type = 1000;
  wallpaper_info_dict.Set(WallpaperInfo::kNewWallpaperTypeNodeName,
                          invalid_wallpaper_type);

  PrefService* syncable_prefs =
      profile_helper_->GetUserPrefServiceSyncable(account_id_1);
  DCHECK(syncable_prefs);
  ScopedDictPrefUpdate wallpaper_update(syncable_prefs,
                                        prefs::kSyncableWallpaperInfo);
  wallpaper_update->Set(account_id_1.GetUserEmail(),
                        std::move(wallpaper_info_dict));
  WallpaperInfo actual_info;
  EXPECT_FALSE(
      pref_manager_->GetSyncedWallpaperInfo(account_id_1, &actual_info));
}

// Verifies that creating a wallpaper info from prefs with an invalid online
// variant type succeeds.
TEST_F(WallpaperPrefManagerTest,
       GetSyncedWallpaperInfo_InvalidOnlineVariantType) {
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo info = InfoWithType(WallpaperType::kOnline);
  std::vector<OnlineWallpaperVariant> variants;
  // Mangles pref data with invalid wallpaper type.
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        static_cast<backdrop::Image::ImageType>(
                            backdrop::Image::IMAGE_TYPE_LIGHT_MODE + 1000));
  info.variants = variants;
  info.collection_id = "_test_collection_id";
  info.asset_id = kAssetId;
  info.unit_id = kAssetId;
  base::Value::Dict wallpaper_info_dict = info.ToDict();

  PrefService* syncable_prefs =
      profile_helper_->GetUserPrefServiceSyncable(account_id_1);
  DCHECK(syncable_prefs);
  ScopedDictPrefUpdate wallpaper_update(syncable_prefs,
                                        prefs::kSyncableWallpaperInfo);
  wallpaper_update->Set(account_id_1.GetUserEmail(),
                        std::move(wallpaper_info_dict));
  WallpaperInfo actual_info;
  EXPECT_TRUE(
      pref_manager_->GetSyncedWallpaperInfo(account_id_1, &actual_info));
}

}  // namespace
}  // namespace ash
