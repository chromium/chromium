// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using sync_timing_helper::TimeMutualSyncCycle;

namespace {

constexpr char kMetricPrefixDictionary[] = "Dictionary.";
constexpr char kMetricAddWordsSyncTime[] = "add_words_sync_time";
constexpr char kMetricRemoveWordsSyncTime[] = "remove_words_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixDictionary, story);
  reporter.RegisterImportantMetric(kMetricAddWordsSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricRemoveWordsSyncTime, "ms");
  return reporter;
}

}  // namespace

class DictionarySyncPerfTest : public SyncTest {
 public:
  DictionarySyncPerfTest() : SyncTest(TWO_CLIENT) {}

  DictionarySyncPerfTest(const DictionarySyncPerfTest&) = delete;
  DictionarySyncPerfTest& operator=(const DictionarySyncPerfTest&) = delete;

  ~DictionarySyncPerfTest() override = default;
};

IN_PROC_BROWSER_TEST_F(DictionarySyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(
      dictionary_helper::DictionaryChecker(/*expected_words=*/{}).Wait());

  perf_test::PerfResultReporter reporter = SetUpReporter(
      base::NumberToString(spellcheck::kMaxSyncableDictionaryWords) + "_words");
  base::TimeDelta dt;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
    ASSERT_TRUE(dictionary_helper::AddWord(0, "foo" + base::NumberToString(i)));
  }
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            dictionary_helper::GetDictionarySize(1));
  reporter.AddResult(kMetricAddWordsSyncTime, dt);

  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
    ASSERT_TRUE(
        dictionary_helper::RemoveWord(0, "foo" + base::NumberToString(i)));
  }
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(1));
  reporter.AddResult(kMetricRemoveWordsSyncTime, dt);
}
