// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <map>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

namespace {

// The tests that are run by this extension are expected to record the following
// user actions, with the specified counts.  If the tests in test.js are
// modified, this array may need to be updated.
struct RecordedUserAction {
  const char* name;
  int count;  // number of times the metric was recorded.
} g_user_actions[] = {
  {"test.ua.1", 1},
  {"test.ua.2", 2},
};

// The tests that are run by this extension are expected to record the following
// histograms.  If the tests in test.js are modified, this array may need to be
// updated.
struct RecordedHistogram {
  const char* name;
  base::HistogramType type;
  int min;
  int max;
  size_t buckets;
  int count;
} g_histograms[] = {
      {"test.h.1", base::HISTOGRAM, 1, 100, 50, 1},          // custom
      {"test.h.2", base::LINEAR_HISTOGRAM, 1, 200, 50, 1},   // custom
      {"test.h.3", base::LINEAR_HISTOGRAM, 1, 101, 102, 2},  // percentage
      {"test.sparse.1", base::SPARSE_HISTOGRAM, 0, 0, 0, 1},
      {"test.sparse.2", base::SPARSE_HISTOGRAM, 0, 0, 0, 2},
      {"test.sparse.3", base::SPARSE_HISTOGRAM, 0, 0, 0, 6},
      {"test.time", base::HISTOGRAM, 1, 10000, 50, 1},
      {"test.medium.time", base::HISTOGRAM, 1, 180000, 50, 1},
      {"test.long.time", base::HISTOGRAM, 1, 3600000, 50, 1},
      {"test.count", base::HISTOGRAM, 1, 1000000, 50, 1},
      {"test.medium.count", base::HISTOGRAM, 1, 10000, 50, 1},
      {"test.small.count", base::HISTOGRAM, 1, 100, 50, 1},
      {"test.bucketchange.linear", base::LINEAR_HISTOGRAM, 1, 100, 10, 2},
      {"test.bucketchange.log", base::HISTOGRAM, 1, 100, 10, 2}, };

// Represents a bucket in a sparse histogram.
struct Bucket {
  int histogram_value;
  int count;
};

// We expect the following sparse histograms.
struct SparseHistogram {
  const char* name;
  int bucket_count;
  Bucket buckets[10];
} g_sparse_histograms[] = {
  {"test.sparse.1", 1, {{42, 1}}},
  {"test.sparse.2", 1, {{24, 2}}},
  {"test.sparse.3", 3, {{1, 1}, {2, 2}, {3, 3}}}};

void ValidateUserActions(const base::UserActionTester& user_action_tester,
                         const RecordedUserAction* recorded,
                         int count) {
  for (int i = 0; i < count; ++i) {
    const RecordedUserAction& ua = recorded[i];
    EXPECT_EQ(ua.count, user_action_tester.GetActionCount(ua.name));
  }
}

void ValidateSparseHistogramSamples(
    const std::string& name,
    const base::HistogramSamples& samples) {
  for (const auto& sparse_histogram : g_sparse_histograms) {
    if (std::string(name) == sparse_histogram.name) {
      for (int j = 0; j < sparse_histogram.bucket_count; ++j) {
        const Bucket& bucket = sparse_histogram.buckets[j];
        EXPECT_EQ(bucket.count, samples.GetCount(bucket.histogram_value));
      }
    }
  }
}

void ValidateHistograms(const RecordedHistogram* recorded,
                        int count) {
  const base::StatisticsRecorder::Histograms histograms =
      base::StatisticsRecorder::GetHistograms();

  // Code other than the tests tun here will record some histogram values, but
  // we will ignore those. This function validates that all the histogram we
  // expect to see are present in the list, and that their basic info is
  // correct.
  for (int i = 0; i < count; ++i) {
    const RecordedHistogram& r = recorded[i];
    size_t j = 0;
    for (j = 0; j < histograms.size(); ++j) {
      base::HistogramBase* histogram(histograms[j]);
      if (std::string(r.name) == histogram->histogram_name()) {
        std::unique_ptr<base::HistogramSamples> snapshot =
            histogram->SnapshotSamples();
        base::HistogramBase::Count sample_count = snapshot->TotalCount();
        EXPECT_EQ(r.count, sample_count);

        EXPECT_EQ(r.type, histogram->GetHistogramType());
        if (r.type == base::SPARSE_HISTOGRAM) {
          ValidateSparseHistogramSamples(r.name, *snapshot);
        } else {
          EXPECT_TRUE(
              histogram->HasConstructionArguments(r.min, r.max, r.buckets));
        }
        break;
      }
    }
    EXPECT_LT(j, histograms.size());
  }
}

}  // namespace

using ContextType = ExtensionBrowserTest::ContextType;

class ExtensionMetricsApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionMetricsApiTest() : ExtensionApiTest(GetParam()) {}
  ~ExtensionMetricsApiTest() override = default;
  ExtensionMetricsApiTest(const ExtensionMetricsApiTest&) = delete;
  ExtensionMetricsApiTest& operator=(const ExtensionMetricsApiTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionMetricsApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionMetricsApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionMetricsApiTest, Metrics) {
  base::UserActionTester user_action_tester;

  base::FieldTrialList::CreateFieldTrial("apitestfieldtrial2", "group1");

  ASSERT_TRUE(base::AssociateFieldTrialParams("apitestfieldtrial2", "group1",
                                              {{"a", "aa"}, {"b", "bb"}}));

  ASSERT_TRUE(RunExtensionTest("metrics", {}, {.load_as_component = true}))
      << message_;

  ValidateUserActions(user_action_tester, g_user_actions,
                      std::size(g_user_actions));
  ValidateHistograms(g_histograms, std::size(g_histograms));
}

}  // namespace extensions
