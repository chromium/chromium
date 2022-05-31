// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_pref_manager.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kUser1[] = "user1@test.com";
const AccountId account_id_1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

WallpaperInfo InfoWithType(WallpaperType type) {
  return WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, type,
                       base::Time::Now());
}

base::Value CreateWallpaperInfoDict(WallpaperInfo info) {
  base::Value wallpaper_info_dict(base::Value::Type::DICTIONARY);
  if (info.asset_id.has_value()) {
    wallpaper_info_dict.SetStringKey(
        WallpaperPrefManager::kNewWallpaperAssetIdNodeName,
        base::NumberToString(info.asset_id.value()));
  }
  if (info.dedup_key.has_value()) {
    wallpaper_info_dict.SetStringKey(
        WallpaperPrefManager::kNewWallpaperDedupKeyNodeName,
        info.dedup_key.value());
  }
  if (info.unit_id.has_value()) {
    wallpaper_info_dict.SetStringKey(
        WallpaperPrefManager::kNewWallpaperUnitIdNodeName,
        base::NumberToString(info.unit_id.value()));
  }
  base::Value online_wallpaper_variant_list(base::Value::Type::LIST);
  for (const auto& variant : info.variants) {
    base::Value online_wallpaper_variant_dict(base::Value::Type::DICTIONARY);
    online_wallpaper_variant_dict.SetStringKey(
        WallpaperPrefManager::kNewWallpaperAssetIdNodeName,
        base::NumberToString(variant.asset_id));
    online_wallpaper_variant_dict.SetStringKey(
        WallpaperPrefManager::kOnlineWallpaperUrlNodeName,
        variant.raw_url.spec());
    online_wallpaper_variant_dict.SetIntKey(
        WallpaperPrefManager::kOnlineWallpaperTypeNodeName,
        static_cast<int>(variant.type));
    online_wallpaper_variant_list.Append(
        std::move(online_wallpaper_variant_dict));
  }
  wallpaper_info_dict.SetKey(
      WallpaperPrefManager::kNewWallpaperVariantListNodeName,
      std::move(online_wallpaper_variant_list));
  wallpaper_info_dict.SetStringKey(
      WallpaperPrefManager::kNewWallpaperCollectionIdNodeName,
      info.collection_id);
  wallpaper_info_dict.SetStringKey(
      WallpaperPrefManager::kNewWallpaperDateNodeName,
      base::NumberToString(info.date.ToInternalValue()));
  wallpaper_info_dict.SetStringKey(
      WallpaperPrefManager::kNewWallpaperLocationNodeName, info.location);
  wallpaper_info_dict.SetIntKey(
      WallpaperPrefManager::kNewWallpaperLayoutNodeName, info.layout);
  wallpaper_info_dict.SetIntKey(WallpaperPrefManager::kNewWallpaperTypeNodeName,
                                static_cast<int>(info.type));
  return wallpaper_info_dict;
}

void PutWallpaperInfoInPrefs(AccountId account_id,
                             WallpaperInfo info,
                             PrefService* pref_service,
                             const std::string& pref_name) {
  DictionaryPrefUpdate wallpaper_update(pref_service, pref_name);
  base::Value wallpaper_info_dict = CreateWallpaperInfoDict(info);
  wallpaper_update->SetKey(account_id.GetUserEmail(),
                           std::move(wallpaper_info_dict));
}

void AssertWallpaperInfoInPrefs(const PrefService* pref_service,
                                const char pref_name[],
                                AccountId account_id,
                                WallpaperInfo info) {
  const base::Value* stored_info_dict =
      pref_service->GetDictionary(pref_name)->FindDictKey(
          account_id.GetUserEmail());
  base::Value expected_info_dict = CreateWallpaperInfoDict(info);
  EXPECT_EQ(expected_info_dict, *stored_info_dict);
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
    if (!is_sync_enabled)
      return nullptr;

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

  bool is_session_started = true;
  bool is_sync_enabled = true;
  AccountId active_account;

 private:
  std::map<AccountId, TestingPrefServiceSimple> synced_prefs_;
};

class WallpaperPrefManagerTest : public testing::Test {
 public:
  WallpaperPrefManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    WallpaperPrefManager::RegisterLocalStatePrefs(local_state_->registry());

    auto profile_helper = std::make_unique<TestProfileHelper>();
    profile_helper_ = profile_helper.get();
    pref_manager_ = WallpaperPrefManager::CreateForTesting(
        local_state_.get(), std::move(profile_helper));
  }

  void TearDown() override {}

  PrefService* GetLocalPrefService() { return local_state_.get(); }

  void SimulateUserLogin(const AccountId& id) {
    profile_helper_->RegisterPrefsForAccount(id);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  TestProfileHelper* profile_helper_;

  TestWallpaperControllerClient client_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;

  std::unique_ptr<WallpaperPrefManager> pref_manager_;
};

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfo_Normal) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, false, expected_info);

  WallpaperInfo actual_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(account_id_1, false, &actual_info));
  EXPECT_EQ(expected_info, actual_info);
}

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfo_Ephemeral) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, true, expected_info);

  WallpaperInfo actual_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(account_id_1, true, &actual_info));
  EXPECT_EQ(expected_info, actual_info);
}

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfoNothingToGet_Normal) {
  WallpaperInfo info;
  EXPECT_FALSE(pref_manager_->GetUserWallpaperInfo(account_id_1, false, &info));
}

TEST_F(WallpaperPrefManagerTest, GetWallpaperInfoNothingToGet_Ephemeral) {
  WallpaperInfo info;
  EXPECT_FALSE(pref_manager_->GetUserWallpaperInfo(account_id_1, true, &info));
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfo_EphemeralDoesNotChangeLocal) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kDaily);
  pref_manager_->SetUserWallpaperInfo(account_id_1, true, expected_info);

  // Local state is expected to be untouched for ephemeral users.
  EXPECT_EQ(nullptr, local_state_->GetUserPrefValue(prefs::kUserWallpaperInfo));
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoLocal) {
  WallpaperInfo info(
      GetDummyFileName(account_id_1), WALLPAPER_LAYOUT_CENTER_CROPPED,
      WallpaperType::kThirdParty, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, false, info));
  AssertWallpaperInfoInPrefs(local_state_.get(), prefs::kUserWallpaperInfo,
                             account_id_1, info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoSynced) {
  base::test::ScopedFeatureList scoped_features(features::kWallpaperWebUI);
  profile_helper_->RegisterPrefsForAccount(account_id_1);

  WallpaperInfo info = InfoWithType(WallpaperType::kOnline);
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, false, info));
  AssertWallpaperInfoInPrefs(
      profile_helper_->GetUserPrefServiceSyncable(account_id_1),
      prefs::kSyncableWallpaperInfo, account_id_1, info);
}

TEST_F(WallpaperPrefManagerTest, SetWallpaperInfoSyncDisabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kWallpaperWebUI);

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
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, false, info));

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
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id_1, false, info));

  // Custom wallpaper infos should not be propagated to synced preferences until
  // the image is uploaded to drivefs. That is not done in
  // |SetUserWallpaperInfo|.
  AssertWallpaperInfoInPrefs(
      profile_helper_->GetUserPrefServiceSyncable(account_id_1),
      prefs::kSyncableWallpaperInfo, account_id_1, synced_info);
}

}  // namespace
}  // namespace ash
