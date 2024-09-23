// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

namespace {

using autofill::AutocompleteKey;
using autofill::AutofillProfile;
using autofill_helper::AddProfile;
using autofill_helper::GetAllAutoFillProfiles;
using autofill_helper::GetKeyCount;
using autofill_helper::GetProfileCount;
using autofill_helper::RemoveKeys;
using autofill_helper::RemoveProfile;
using autofill_helper::UpdateProfile;
using sync_timing_helper::TimeMutualSyncCycle;

// These numbers should be as far away from a multiple of
// `kDefaultMaxCommitBatchSize` as possible, so that sync cycle counts
// for batch operations stay the same even if some batches end up not
// being completely full.
constexpr size_t kNumKeys = 163;
constexpr size_t kNumProfiles = 163;
// Checks that the given `item_count` is right in the middle between two
// multiples of `kDefaultMaxCommitBatchSize`.
constexpr bool IsRightBetweenCommitBatches(size_t item_count) {
  size_t item_count_in_last_commit =
      item_count % syncer::kDefaultMaxCommitBatchSize;
  size_t min_good_count = syncer::kDefaultMaxCommitBatchSize / 2;
  size_t max_good_count = (syncer::kDefaultMaxCommitBatchSize + 1) / 2;
  return min_good_count <= item_count_in_last_commit &&
         item_count_in_last_commit <= max_good_count;
}
static_assert(
    IsRightBetweenCommitBatches(kNumKeys),
    "kNumKeys should be between two multiples of kDefaultMaxCommitBatchSize");
static_assert(IsRightBetweenCommitBatches(kNumProfiles),
              "kNumProfiles should be between two multiples of "
              "kDefaultMaxCommitBatchSize");

constexpr char kMetricPrefixAutofill[] = "Autofill.";
constexpr char kMetricAddProfilesSyncTime[] = "add_profiles_sync_time";
constexpr char kMetricUpdateProfilesSyncTime[] = "update_profiles_sync_time";
constexpr char kMetricDeleteProfilesSyncTime[] = "delete_profiles_sync_time";
constexpr char kMetricAddKeysSyncTime[] = "add_keys_sync_time";
constexpr char kMetricDeleteKeysSyncTime[] = "delete_keys_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixAutofill, story);
  reporter.RegisterImportantMetric(kMetricAddProfilesSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricUpdateProfilesSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteProfilesSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricAddKeysSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteKeysSyncTime, "ms");
  return reporter;
}

std::string IntToName(int n) {
  return base::StringPrintf("Name%d", n);
}

void ForceSync(int profile) {
  static size_t id = 0;
  ++id;
  EXPECT_TRUE(bookmarks_helper::AddURL(
                  profile, 0, bookmarks_helper::IndexedURLTitle(id),
                  GURL(bookmarks_helper::IndexedURL(id))) != nullptr);
}

class AutofillProfileSyncPerfTest : public SyncTest {
 public:
  AutofillProfileSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  AutofillProfileSyncPerfTest(const AutofillProfileSyncPerfTest&) = delete;
  AutofillProfileSyncPerfTest& operator=(const AutofillProfileSyncPerfTest&) =
      delete;

  // Adds |num_profiles| new autofill profiles to the sync profile |profile|.
  void AddProfiles(int profile, int num_profiles);

  // Updates all autofill profiles for the sync profile |profile|.
  void UpdateProfiles(int profile);

  // Removes all autofill profiles from |profile|.
  void RemoveProfiles(int profile);

 private:
  // Returns a new unique autofill profile.
  const AutofillProfile NextAutofillProfile();

  // Returns an unused unique guid.
  const std::string NextGUID();

  // Returns a unique guid based on the input integer |n|.
  const std::string IntToGUID(int n);

  // Returns a new unused unique name.
  const std::string NextName();

  int guid_number_ = 0;
  int name_number_ = 0;
};

void AutofillProfileSyncPerfTest::AddProfiles(int profile, int num_profiles) {
  for (int i = 0; i < num_profiles; ++i) {
    AddProfile(profile, NextAutofillProfile());
  }
}

void AutofillProfileSyncPerfTest::UpdateProfiles(int profile) {
  // Since `UpdateProfile()` invalidates the pointers returned by
  // `GetAllAutoFillProfiles()`, collect the `guids` first.
  std::vector<std::string> guids;
  for (const AutofillProfile* autofill_profile :
       GetAllAutoFillProfiles(profile)) {
    guids.push_back(autofill_profile->guid());
  }
  for (const std::string& guid : guids) {
    UpdateProfile(profile, guid, autofill::NAME_FIRST,
                  base::UTF8ToUTF16(NextName()));
  }
}

void AutofillProfileSyncPerfTest::RemoveProfiles(int profile) {
  // Since `RemoveProfile()` invalidates the pointers returned by
  // `GetAllAutoFillProfiles()`, collect the `guids` first.
  std::vector<std::string> guids;
  for (const AutofillProfile* autofill_profile :
       GetAllAutoFillProfiles(profile)) {
    guids.push_back(autofill_profile->guid());
  }
  for (const std::string& guid : guids) {
    RemoveProfile(profile, guid);
  }
}

const AutofillProfile AutofillProfileSyncPerfTest::NextAutofillProfile() {
  AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  autofill::test::SetProfileInfoWithGuid(&profile, NextGUID().c_str(),
                                         NextName().c_str(), "", "", "", "", "",
                                         "", "", "", "", "", "");
  return profile;
}

const std::string AutofillProfileSyncPerfTest::NextGUID() {
  return IntToGUID(guid_number_++);
}

const std::string AutofillProfileSyncPerfTest::IntToGUID(int n) {
  return base::StringPrintf("00000000-0000-0000-0000-%012X", n);
}

const std::string AutofillProfileSyncPerfTest::NextName() {
  return IntToName(name_number_++);
}

IN_PROC_BROWSER_TEST_F(AutofillProfileSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumProfiles) + "_profiles");
  AddProfiles(0, kNumProfiles);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(1));
  reporter.AddResult(kMetricAddProfilesSyncTime, dt);

  UpdateProfiles(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(1));
  reporter.AddResult(kMetricUpdateProfilesSyncTime, dt);

  RemoveProfiles(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0U, GetProfileCount(1));
  reporter.AddResult(kMetricDeleteProfilesSyncTime, dt);
}

class AutocompleteSyncPerfTest : public SyncTest {
 public:
  AutocompleteSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  AutocompleteSyncPerfTest(const AutocompleteSyncPerfTest&) = delete;
  AutocompleteSyncPerfTest& operator=(const AutocompleteSyncPerfTest&) = delete;

  // Adds |num_keys| new autocomplete keys to the sync profile |profile|.
  void AddKeys(int profile, int num_keys);

 private:
  // Returns a new unique autocomplete key.
  const AutocompleteKey NextAutocompleteKey();

  // Returns a new unused unique name.
  const std::string NextName();

  int name_number_ = 0;
};

void AutocompleteSyncPerfTest::AddKeys(int profile, int num_keys) {
  std::set<AutocompleteKey> keys;
  for (int i = 0; i < num_keys; ++i) {
    keys.insert(NextAutocompleteKey());
  }
  autofill_helper::AddKeys(profile, keys);
}

const AutocompleteKey AutocompleteSyncPerfTest::NextAutocompleteKey() {
  return AutocompleteKey(NextName().c_str(), NextName().c_str());
}

const std::string AutocompleteSyncPerfTest::NextName() {
  return IntToName(name_number_++);
}

IN_PROC_BROWSER_TEST_F(AutocompleteSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumKeys) + "_keys");
  AddKeys(0, kNumKeys);
  // TODO(lipalani): fix this. The following line is added to force sync.
  ForceSync(0);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumKeys, GetKeyCount(1));
  reporter.AddResult(kMetricAddKeysSyncTime, dt);

  RemoveKeys(0);
  // TODO(lipalani): fix this. The following line is added to force sync.
  ForceSync(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0U, GetKeyCount(1));
  reporter.AddResult(kMetricDeleteKeysSyncTime, dt);
}

}  // namespace
