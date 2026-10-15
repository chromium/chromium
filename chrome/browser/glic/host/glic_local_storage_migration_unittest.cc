// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_local_storage_migration.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_client_data_storage.h"
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
  }
  ~GlicLocalStorageMigrationTest() override = default;

  content::StoragePartition* GetGlicPartition() {
    return profile_.GetStoragePartition(
        GetGlicStoragePartitionConfig(&profile_));
  }

  void PutLatin1InPartition(content::StoragePartition* partition,
                            const blink::StorageKey& storage_key,
                            std::string_view key,
                            std::string_view value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    partition->GetLocalStorageControl()->BindStorageArea(
        storage_key, area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> key_vec;
    key_vec.push_back(0x01);
    key_vec.insert(key_vec.end(), key.begin(), key.end());
    std::vector<uint8_t> val_vec;
    val_vec.push_back(0x01);
    val_vec.insert(val_vec.end(), value.begin(), value.end());
    base::RunLoop loop;
    area->Put(key_vec, val_vec, std::nullopt, nullptr,
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                loop.Quit();
              }));
    loop.Run();
  }

  void PutUtf16InPartition(content::StoragePartition* partition,
                           const blink::StorageKey& storage_key,
                           std::string_view key,
                           std::u16string_view value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    partition->GetLocalStorageControl()->BindStorageArea(
        storage_key, area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> key_vec;
    key_vec.push_back(0x01);
    key_vec.insert(key_vec.end(), key.begin(), key.end());
    std::vector<uint8_t> val_vec;
    val_vec.push_back(0x00);
    auto val_bytes = base::as_byte_span(value);
    val_vec.insert(val_vec.end(), val_bytes.begin(), val_bytes.end());
    base::RunLoop loop;
    area->Put(key_vec, val_vec, std::nullopt, nullptr,
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                loop.Quit();
              }));
    loop.Run();
  }

  std::optional<std::string> GetFromPrefs(std::string_view key) {
    client_data_storage_.Flush();
    const std::string* val = profile_.GetPrefs()
                                 ->GetDict(prefs::kGlicClientDataStorage)
                                 .FindString(key);
    if (!val) {
      return std::nullopt;
    }
    return *val;
  }

  std::optional<std::string> GetFromStorage(std::string_view key) {
    const auto& all = client_data_storage_.GetAll();
    auto it = all.find(key);
    if (it == all.end()) {
      return std::nullopt;
    }
    return it->second;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  TestingProfile profile_;
  GlicClientDataStorage client_data_storage_{profile_.GetPrefs()};
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
};

TEST_F(GlicLocalStorageMigrationTest, MigratesKeysAndSetsPref) {
  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(prefs::kGlicLocalStorageCopiedToPrefs));

  GURL guest_url = GetGuestURL(&profile_);
  ASSERT_TRUE(guest_url.is_valid());
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(guest_url));

  PutLatin1InPartition(GetGlicPartition(), storage_key,
                       "BARD_EMBED_CHAT_STORAGE_KEY_V2", "bard_value");
  PutUtf16InPartition(GetGlicPartition(), storage_key,
                      "WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE",
                      u"capabilities_val_\u2603");
  PutLatin1InPartition(GetGlicPartition(), storage_key, "OTHER_UNRELATED_KEY",
                       "unrelated_val");

  MaybeMigrateGlicLocalStorage(&profile_, &client_data_storage_);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return profile_.GetPrefs()->GetBoolean(
        prefs::kGlicLocalStorageCopiedToPrefs);
  }));

  EXPECT_EQ(GetFromStorage("BARD_EMBED_CHAT_STORAGE_KEY_V2"), "bard_value");
  EXPECT_EQ(GetFromPrefs("BARD_EMBED_CHAT_STORAGE_KEY_V2"), "bard_value");
  EXPECT_EQ(GetFromStorage("WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE"),
            "capabilities_val_\xE2\x98\x83");
  EXPECT_EQ(GetFromPrefs("WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE"),
            "capabilities_val_\xE2\x98\x83");
  EXPECT_FALSE(GetFromStorage("OTHER_UNRELATED_KEY").has_value());
  EXPECT_FALSE(GetFromPrefs("OTHER_UNRELATED_KEY").has_value());
}

TEST_F(GlicLocalStorageMigrationTest, PreservesExistingPrefValues) {
  client_data_storage_.SetItem("BARD_EMBED_CHAT_STORAGE_KEY_V2",
                               "newer_pref_value");

  GURL guest_url = GetGuestURL(&profile_);
  ASSERT_TRUE(guest_url.is_valid());
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(guest_url));

  PutLatin1InPartition(GetGlicPartition(), storage_key,
                       "BARD_EMBED_CHAT_STORAGE_KEY_V2", "older_glicpart_val");
  PutLatin1InPartition(GetGlicPartition(), storage_key,
                       "WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE",
                       "capabilities_val");

  MaybeMigrateGlicLocalStorage(&profile_, &client_data_storage_);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return profile_.GetPrefs()->GetBoolean(
        prefs::kGlicLocalStorageCopiedToPrefs);
  }));

  EXPECT_EQ(GetFromStorage("BARD_EMBED_CHAT_STORAGE_KEY_V2"),
            "newer_pref_value");
  EXPECT_EQ(GetFromPrefs("BARD_EMBED_CHAT_STORAGE_KEY_V2"), "newer_pref_value");
  EXPECT_EQ(GetFromStorage("WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE"),
            "capabilities_val");
  EXPECT_EQ(GetFromPrefs("WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE"),
            "capabilities_val");
}

TEST_F(GlicLocalStorageMigrationTest, NoOpIfPrefAlreadySet) {
  profile_.GetPrefs()->SetBoolean(prefs::kGlicLocalStorageCopiedToPrefs, true);

  GURL guest_url = GetGuestURL(&profile_);
  ASSERT_TRUE(guest_url.is_valid());
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(guest_url));

  PutLatin1InPartition(GetGlicPartition(), storage_key,
                       "BARD_EMBED_CHAT_STORAGE_KEY_V2", "bard_value");

  MaybeMigrateGlicLocalStorage(&profile_, &client_data_storage_);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return task_environment_.MainThreadIsIdle(); }));

  EXPECT_FALSE(GetFromStorage("BARD_EMBED_CHAT_STORAGE_KEY_V2").has_value());
  EXPECT_FALSE(GetFromPrefs("BARD_EMBED_CHAT_STORAGE_KEY_V2").has_value());
}

TEST_F(GlicLocalStorageMigrationTest, NoGlicPartitionSetsPref) {
  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(prefs::kGlicLocalStorageCopiedToPrefs));

  MaybeMigrateGlicLocalStorage(&profile_, &client_data_storage_);
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kGlicLocalStorageCopiedToPrefs));
}

TEST_F(GlicLocalStorageMigrationTest, EmptyGlicPartitionSetsPref) {
  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(prefs::kGlicLocalStorageCopiedToPrefs));

  GetGlicPartition();

  MaybeMigrateGlicLocalStorage(&profile_, &client_data_storage_);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return profile_.GetPrefs()->GetBoolean(
        prefs::kGlicLocalStorageCopiedToPrefs);
  }));
}

}  // namespace
}  // namespace glic
