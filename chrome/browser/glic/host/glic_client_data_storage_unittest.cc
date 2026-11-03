// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_client_data_storage.h"

#include <optional>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

constexpr char kGuestUrl[] = "https://example.com/side-panel?configId=abc";
constexpr char kOtherGuestUrl[] = "https://example.com/side-panel?configId=xyz";

class GlicClientDataStorageTest : public testing::Test {
 public:
  GlicClientDataStorageTest() {
    prefs_.registry()->RegisterDictionaryPref(prefs::kGlicClientDataStorage);
  }

 protected:
  const base::DictValue& pref() {
    return prefs_.GetDict(prefs::kGlicClientDataStorage);
  }

  // Returns the pref without the version key.
  base::DictValue Entries() {
    base::DictValue entries = pref().Clone();
    entries.Remove(kClientDataStorageVersionKey);
    return entries;
  }

  void SetEntry(const std::string& guest_url_key, base::DictValue entry) {
    ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
        ->Set(guest_url_key, std::move(entry));
  }

  // Marks the pref as already migrated.
  void SetVersion() {
    ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
        ->Set(kClientDataStorageVersionKey, 2);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple prefs_;
};

// Consumer items stored by older versions at the top level move to the Gemini
// key.
TEST_F(GlicClientDataStorageTest, MigratesV1ItemsToGeminiKey) {
  ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
      ->Set("onboarded", "true");

  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  EXPECT_EQ(storage.GetAll().size(), 1u);
  EXPECT_TRUE(storage.Contains("onboarded"));
  EXPECT_EQ(pref(), base::DictValue()
                        .Set(kClientDataStorageVersionKey, 2)
                        .Set(kGeminiGuestUrlKey,
                             base::DictValue().Set("onboarded", "true")));
}

// Constructing the storage on a fresh profile doesn't write the pref.
TEST_F(GlicClientDataStorageTest, FreshProfileDoesNotWritePref) {
  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  EXPECT_TRUE(storage.GetAll().empty());
  EXPECT_FALSE(prefs_.GetUserPrefValue(prefs::kGlicClientDataStorage));
}

TEST_F(GlicClientDataStorageTest, FirstWriteSetsVersion) {
  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  storage.SetItem("onboarded", "true");
  storage.Flush();
  EXPECT_EQ(pref(), base::DictValue()
                        .Set(kClientDataStorageVersionKey, 2)
                        .Set(kGeminiGuestUrlKey,
                             base::DictValue().Set("onboarded", "true")));
}

// v1 items are merged into an existing Gemini key entry, if any.
TEST_F(GlicClientDataStorageTest, MigrationMergesIntoExistingGeminiEntry) {
  ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
      ->Set("onboarded", "true");
  SetEntry(kGeminiGuestUrlKey, base::DictValue().Set("dismissed", "true"));

  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  EXPECT_EQ(
      Entries(),
      base::DictValue().Set(
          kGeminiGuestUrlKey,
          base::DictValue().Set("dismissed", "true").Set("onboarded", "true")));
}

// v1 items are migrated to the Gemini key even if the profile uses another
// guest URL key.
TEST_F(GlicClientDataStorageTest, MigrationRunsForOtherGuestUrlKey) {
  ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
      ->Set("onboarded", "true");

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  EXPECT_TRUE(storage.GetAll().empty());
  EXPECT_EQ(Entries(),
            base::DictValue().Set(kGeminiGuestUrlKey,
                                  base::DictValue().Set("onboarded", "true")));
}

// v1 only wrote strings, so other top-level values are dropped.
TEST_F(GlicClientDataStorageTest, MigrationDropsNonStringV1Values) {
  {
    ScopedDictPrefUpdate update(&prefs_, prefs::kGlicClientDataStorage);
    update->Set("str", "1");
    update->Set("bool", true);
    update->Set("int", 5);
  }

  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  EXPECT_EQ(Entries(),
            base::DictValue().Set(kGeminiGuestUrlKey,
                                  base::DictValue().Set("str", "1")));
}

TEST_F(GlicClientDataStorageTest, MigrationKeepsNestedEntries) {
  ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
      ->Set("onboarded", "true");
  SetEntry(kGuestUrl, base::DictValue().Set("dismissed", "true"));

  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  EXPECT_EQ(
      Entries(),
      base::DictValue()
          .Set(kGeminiGuestUrlKey, base::DictValue().Set("onboarded", "true"))
          .Set(kGuestUrl, base::DictValue().Set("dismissed", "true")));
}

TEST_F(GlicClientDataStorageTest, MigrationRunsOnce) {
  SetVersion();
  ScopedDictPrefUpdate(&prefs_, prefs::kGlicClientDataStorage)
      ->Set("top_level", "value");

  GlicClientDataStorage storage(&prefs_, kGeminiGuestUrlKey);
  EXPECT_TRUE(storage.GetAll().empty());
  EXPECT_EQ(Entries(), base::DictValue().Set("top_level", "value"));
}

TEST_F(GlicClientDataStorageTest, ReadsOnlyItsEntry) {
  SetEntry(kGuestUrl, base::DictValue().Set("dismissed", "true"));
  SetEntry(kOtherGuestUrl, base::DictValue().Set("other", "value"));

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  EXPECT_EQ(storage.GetAll().size(), 1u);
  EXPECT_TRUE(storage.Contains("dismissed"));
  EXPECT_FALSE(storage.Contains("other"));
}

TEST_F(GlicClientDataStorageTest, IgnoresNonStringValues) {
  SetEntry(kGuestUrl, base::DictValue().Set("bool", true).Set("str", "true"));

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  EXPECT_FALSE(storage.Contains("bool"));
  EXPECT_TRUE(storage.Contains("str"));
}

TEST_F(GlicClientDataStorageTest, WriteKeepsOtherEntries) {
  SetEntry(kOtherGuestUrl, base::DictValue().Set("other", "value"));

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  storage.SetItem("dismissed", "true");
  storage.Flush();

  EXPECT_EQ(Entries(),
            base::DictValue()
                .Set(kGuestUrl, base::DictValue().Set("dismissed", "true"))
                .Set(kOtherGuestUrl, base::DictValue().Set("other", "value")));
}

TEST_F(GlicClientDataStorageTest, WriteIsDelayed) {
  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  storage.SetItem("dismissed", "true");
  EXPECT_FALSE(pref().FindDict(kGuestUrl));

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(pref().FindDict(kGuestUrl));
}

TEST_F(GlicClientDataStorageTest, DestructorFlushesPendingWrite) {
  {
    GlicClientDataStorage storage(&prefs_, kGuestUrl);
    storage.SetItem("dismissed", "true");
    ASSERT_FALSE(pref().FindDict(kGuestUrl));
  }
  EXPECT_EQ(Entries(),
            base::DictValue().Set(kGuestUrl,
                                  base::DictValue().Set("dismissed", "true")));
}

TEST_F(GlicClientDataStorageTest, ClearRemovesOnlyItsEntry) {
  SetEntry(kGuestUrl, base::DictValue().Set("dismissed", "true"));
  SetEntry(kOtherGuestUrl, base::DictValue().Set("other", "value"));

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  storage.Clear();
  storage.Flush();

  EXPECT_EQ(Entries(),
            base::DictValue().Set(kOtherGuestUrl,
                                  base::DictValue().Set("other", "value")));
}

TEST_F(GlicClientDataStorageTest, RemovingLastItemRemovesEntry) {
  SetEntry(kGuestUrl, base::DictValue().Set("dismissed", "true"));
  SetEntry(kOtherGuestUrl, base::DictValue().Set("other", "value"));

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  storage.SetItem("dismissed", std::nullopt);
  storage.Flush();

  EXPECT_EQ(Entries(),
            base::DictValue().Set(kOtherGuestUrl,
                                  base::DictValue().Set("other", "value")));
}

// The limit covers all guest URL keys. Exceeding it clears the whole pref.
TEST_F(GlicClientDataStorageTest, SizeLimitAppliesToWholePref) {
  const std::string big_value(kMaxTotalClientDataStorageBytes / 2, 'a');
  SetEntry(kOtherGuestUrl, base::DictValue().Set("big", big_value));

  GlicClientDataStorage storage(&prefs_, kGuestUrl);
  storage.SetItem("small", "value");
  storage.Flush();
  ASSERT_TRUE(pref().FindDict(kGuestUrl));

  storage.SetItem("big", big_value);
  storage.Flush();
  EXPECT_TRUE(storage.GetAll().empty());
  EXPECT_EQ(pref(), base::DictValue().Set(kClientDataStorageVersionKey, 2));
}

}  // namespace
}  // namespace glic
