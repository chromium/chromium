// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/low_trust_policy_install_block_manager.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_extension_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kBlockedLowTrustPolicyExtensions[] =
    "extensions.blocked_low_trust_policy_installs";
constexpr char kOverrideTypeKey[] = "override_type";
constexpr char kUpdateUrlKey[] = "update_url";
constexpr char kTimestampKey[] = "timestamp";

constexpr char kStaleId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kFreshId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr char kMissingTimestampId[] = "cccccccccccccccccccccccccccccccc";
constexpr char kMissingUpdateUrlId[] = "dddddddddddddddddddddddddddddddd";
constexpr char kInvalidEnumId[] = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
constexpr char kNonDictId[] = "ffffffffffffffffffffffffffffffff";

}  // namespace

class LowTrustPolicyInstallBlockManagerTest : public testing::Test {
 public:
  LowTrustPolicyInstallBlockManagerTest() = default;

  void SetUp() override {
    test_prefs_ = std::make_unique<TestExtensionPrefs>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        std::make_unique<TestingProfile>());
    LowTrustPolicyInstallBlockManager::RegisterProfilePrefs(
        test_prefs_->pref_registry().get());
    manager_ = std::make_unique<LowTrustPolicyInstallBlockManager>(
        *test_prefs_->prefs()->pref_service());
  }

 protected:
  PrefService* pref_service() { return test_prefs_->prefs()->pref_service(); }
  LowTrustPolicyInstallBlockManager* manager() { return manager_.get(); }

  // Populates the preference store with a representative set of entries for
  // testing: a fresh valid entry, a stale (expired TTL) entry, and various
  // malformed records (missing timestamp, missing update URL, invalid enum
  // value, and non-dictionary value).
  void PopulateTestEntries(base::Time now) {
    base::TimeDelta ttl = LowTrustPolicyInstallBlockManager::GetTTLForTesting();

    manager()->MarkBlocked(
        kStaleId,
        BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kDse,
                             .update_url = "http://example1.com",
                             .timestamp = now - (ttl + base::Days(1))});

    manager()->MarkBlocked(
        kFreshId,
        BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kNtp,
                             .update_url = "http://example2.com",
                             .timestamp = now - (ttl / 2)});

    ScopedDictPrefUpdate update(pref_service(),
                                kBlockedLowTrustPolicyExtensions);

    base::DictValue missing_timestamp_entry;
    missing_timestamp_entry.Set(kOverrideTypeKey, 1);
    missing_timestamp_entry.Set(kUpdateUrlKey, "http://example.com");
    update->Set(kMissingTimestampId, std::move(missing_timestamp_entry));

    base::DictValue missing_update_url_entry;
    missing_update_url_entry.Set(kOverrideTypeKey, 1);
    missing_update_url_entry.Set(kTimestampKey, base::TimeToValue(now));
    update->Set(kMissingUpdateUrlId, std::move(missing_update_url_entry));

    base::DictValue invalid_enum_entry;
    invalid_enum_entry.Set(kOverrideTypeKey, 999);
    invalid_enum_entry.Set(kUpdateUrlKey, "http://example.com");
    invalid_enum_entry.Set(kTimestampKey, base::TimeToValue(now));
    update->Set(kInvalidEnumId, std::move(invalid_enum_entry));

    update->Set(kNonDictId, "invalid_string_value");
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestExtensionPrefs> test_prefs_;
  std::unique_ptr<LowTrustPolicyInstallBlockManager> manager_;
};

// Verifies basic lifecycle operations: marking an extension as blocked,
// querying blocked state, updating metadata, and clearing records.
TEST_F(LowTrustPolicyInstallBlockManagerTest, BlockedByLowTrust) {
  constexpr char kTestId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  constexpr char kTestId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  constexpr char kNonExistentId[] = "cccccccccccccccccccccccccccccccc";
  base::Time now = base::Time::Now();

  manager()->MarkBlocked(
      kTestId1,
      BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kDse,
                           .update_url = "http://example1.com",
                           .timestamp = now});

  EXPECT_TRUE(manager()->IsBlocked(kTestId1));
  EXPECT_FALSE(manager()->IsBlocked(kTestId2));

  auto map = manager()->GetAllBlocked();
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[kTestId1].override_type, util::DseNtpOverrideType::kDse);
  EXPECT_EQ(map[kTestId1].update_url, "http://example1.com");
  EXPECT_EQ(map[kTestId1].timestamp, now);

  // Clearing a non-existent ID should be a safe no-op.
  manager()->Clear(kNonExistentId);
  EXPECT_FALSE(manager()->IsBlocked(kNonExistentId));

  manager()->Clear(kTestId1);
  EXPECT_FALSE(manager()->IsBlocked(kTestId1));
  EXPECT_TRUE(manager()->GetAllBlocked().empty());

  manager()->MarkBlocked(
      kTestId1,
      BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kNtp,
                           .update_url = "http://example2.com",
                           .timestamp = now});
  EXPECT_TRUE(manager()->IsBlocked(kTestId1));
  map = manager()->GetAllBlocked();
  EXPECT_EQ(map[kTestId1].override_type, util::DseNtpOverrideType::kNtp);
  EXPECT_EQ(map[kTestId1].update_url, "http://example2.com");
}

// Verifies that query methods filter out stale records (expired TTLs) and
// malformed preference entries (missing timestamps, missing update URLs,
// invalid enum values, or non-dictionary structures) while preserving valid,
// active entries.
TEST_F(LowTrustPolicyInstallBlockManagerTest,
       QueryFiltersStaleAndMalformedEntries) {
  base::Time now = base::Time::Now();
  PopulateTestEntries(now);

  EXPECT_FALSE(manager()->IsBlocked(kStaleId));
  EXPECT_FALSE(manager()->IsBlocked(kMissingTimestampId));
  EXPECT_FALSE(manager()->IsBlocked(kMissingUpdateUrlId));
  EXPECT_FALSE(manager()->IsBlocked(kInvalidEnumId));
  EXPECT_FALSE(manager()->IsBlocked(kNonDictId));
  EXPECT_TRUE(manager()->IsBlocked(kFreshId));

  auto map = manager()->GetAllBlocked();
  EXPECT_EQ(map.size(), 1u);
  EXPECT_TRUE(map.contains(kFreshId));
  EXPECT_FALSE(map.contains(kStaleId));
  EXPECT_FALSE(map.contains(kMissingTimestampId));
  EXPECT_FALSE(map.contains(kMissingUpdateUrlId));
  EXPECT_FALSE(map.contains(kInvalidEnumId));
  EXPECT_FALSE(map.contains(kNonDictId));
}

// Verifies that CleanupStaleRecords purges all categories of stale and
// malformed records (expired TTLs, missing fields, invalid enum values, and
// non-dictionary structures) while retaining valid entries.
TEST_F(LowTrustPolicyInstallBlockManagerTest, CleanupStaleRecords) {
  base::Time now = base::Time::Now();
  PopulateTestEntries(now);

  // All 5 stale and malformed records should be purged in a single pass.
  EXPECT_EQ(manager()->CleanupStaleRecords(), 5u);
  EXPECT_EQ(manager()->CleanupStaleRecords(), 0u);

  EXPECT_TRUE(manager()->IsBlocked(kFreshId));
}

}  // namespace extensions
