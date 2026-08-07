// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_local_storage_migration.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace glic {
namespace {

class GlicLocalStorageMigrationTest : public testing::Test {
 public:
  GlicLocalStorageMigrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicNoWebview, features::kGlic}, {});
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
  }
  ~GlicLocalStorageMigrationTest() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  content::StoragePartition* GetGlicPartition() {
    return profile_.GetStoragePartition(
        GetGlicStoragePartitionConfig(&profile_));
  }

  content::StoragePartition* GetDefaultPartition() {
    return profile_.GetDefaultStoragePartition();
  }

  void PutInPartition(content::StoragePartition* partition,
                      const blink::StorageKey& storage_key,
                      std::string_view key,
                      std::string_view value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    partition->GetLocalStorageControl()->BindStorageArea(
        storage_key, area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> key_vec;
    key_vec.push_back(0x01);
    key_vec.insert(key_vec.end(), key.begin(), key.end());
    std::vector<uint8_t> val_vec(value.begin(), value.end());
    base::RunLoop loop;
    area->Put(key_vec, val_vec, std::nullopt, nullptr,
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                loop.Quit();
              }));
    loop.Run();
  }

  std::optional<std::vector<uint8_t>> GetFromPartition(
      content::StoragePartition* partition,
      const blink::StorageKey& storage_key,
      std::string_view key) {
    mojo::Remote<blink::mojom::StorageArea> area;
    partition->GetLocalStorageControl()->BindStorageArea(
        storage_key, area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> key_vec;
    key_vec.push_back(0x01);
    key_vec.insert(key_vec.end(), key.begin(), key.end());
    std::optional<std::vector<uint8_t>> result;
    base::RunLoop loop;
    area->GetAll(
        /*new_observer=*/mojo::NullRemote(),
        base::BindLambdaForTesting(
            [&](std::vector<blink::mojom::KeyValuePtr> data) {
              for (const auto& key_value : data) {
                if (key_value->key == key_vec) {
                  result = key_value->value;
                  break;
                }
              }
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  TestingProfile profile_;
};

TEST_F(GlicLocalStorageMigrationTest, MigratesKeysAndSetsPref) {
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(
      prefs::kGlicLocalStorageCopiedToMainPartition));

  GURL guest_url = GetGuestURL();
  ASSERT_TRUE(guest_url.is_valid());
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(guest_url));

  PutInPartition(GetGlicPartition(), storage_key,
                 "BARD_EMBED_CHAT_STORAGE_KEY_V2", "bard_value");
  PutInPartition(GetGlicPartition(), storage_key,
                 "WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE",
                 "capabilities_val");
  PutInPartition(GetGlicPartition(), storage_key, "OTHER_UNRELATED_KEY",
                 "unrelated_val");

  MaybeMigrateGlicLocalStorage(&profile_);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return profile_.GetPrefs()->GetBoolean(
        prefs::kGlicLocalStorageCopiedToMainPartition);
  }));

  auto bard_val = GetFromPartition(GetDefaultPartition(), storage_key,
                                   "BARD_EMBED_CHAT_STORAGE_KEY_V2");
  ASSERT_TRUE(bard_val.has_value());
  EXPECT_EQ(std::string(bard_val->begin(), bard_val->end()), "bard_value");

  auto caps_val = GetFromPartition(GetDefaultPartition(), storage_key,
                                   "WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE");
  ASSERT_TRUE(caps_val.has_value());
  EXPECT_EQ(std::string(caps_val->begin(), caps_val->end()),
            "capabilities_val");

  auto other_val = GetFromPartition(GetDefaultPartition(), storage_key,
                                    "OTHER_UNRELATED_KEY");
  EXPECT_FALSE(other_val.has_value());
}

TEST_F(GlicLocalStorageMigrationTest, NoOpIfPrefAlreadySet) {
  profile_.GetPrefs()->SetBoolean(prefs::kGlicLocalStorageCopiedToMainPartition,
                                  true);

  GURL guest_url = GetGuestURL();
  ASSERT_TRUE(guest_url.is_valid());
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(guest_url));

  PutInPartition(GetGlicPartition(), storage_key,
                 "BARD_EMBED_CHAT_STORAGE_KEY_V2", "bard_value");

  MaybeMigrateGlicLocalStorage(&profile_);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return task_environment_.MainThreadIsIdle(); }));

  auto bard_val = GetFromPartition(GetDefaultPartition(), storage_key,
                                   "BARD_EMBED_CHAT_STORAGE_KEY_V2");
  EXPECT_FALSE(bard_val.has_value());
}

TEST_F(GlicLocalStorageMigrationTest, EmptyGlicPartitionSetsPref) {
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(
      prefs::kGlicLocalStorageCopiedToMainPartition));

  MaybeMigrateGlicLocalStorage(&profile_);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return profile_.GetPrefs()->GetBoolean(
        prefs::kGlicLocalStorageCopiedToMainPartition);
  }));
}

}  // namespace
}  // namespace glic
