// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_metrics_collector.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::TimeTicks kNavigationStartTime = base::TimeTicks();

const ukm::SourceId kID(1);

const char kEventName[] = "PrefetchProxy.PrefetchedResource";

const std::vector<std::string> kAllEventMetrics{
    "DataLength",   "FetchDurationMS", "ISPFilteringStatus",
    "LinkClicked",  "LinkPosition",    "NavigationStartToFetchStartMS",
    "ResourceType", "Status",
};

network::mojom::URLResponseHeadPtr MakeHead(absl::optional<std::string> headers,
                                            base::TimeDelta request_start) {
  auto head = network::mojom::URLResponseHead::New();
  head->load_timing.request_start = kNavigationStartTime + request_start;
  if (headers) {
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(*headers);
  }
  return head;
}

network::URLLoaderCompletionStatus MakeCompletionStatus(
    net::Error error_code,
    int64_t data_length,
    base::TimeDelta completion_time) {
  network::URLLoaderCompletionStatus status(error_code);
  status.encoded_data_length = data_length;
  status.completion_time = kNavigationStartTime + completion_time;
  return status;
}

// Reading the output of |testing::UnorderedElementsAreArray| is impossible.
std::string ActualUkmEntriesToDebugString(
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries) {
  std::string result = "Actual Entries:\n";

  if (entries.empty()) {
    result = "<empty>";
  }

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    result += base::StringPrintf("=== Entry #%zu\n", i);
    result += base::StringPrintf("Source ID: %d\n",
                                 static_cast<int>(entry.source_id));
    for (const auto& metric : entry.metrics) {
      result += base::StringPrintf("Metric '%s' = %d\n", metric.first.c_str(),
                                   static_cast<int>(metric.second));
    }
    result += "\n";
  }
  result += "\n";
  return result;
}

}  // namespace

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;

class PrefetchProxyPrefetchMetricsCollectorTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, MainframesNotEligible) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);
    collector->OnMainframeResourceNotEligible(
        GURL("http://not-navigated.com"),
        /*prediction_position=*/0,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);

    collector->OnMainframeResourceNotEligible(
        GURL("http://navigated.com"),
        /*prediction_position=*/1,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);

    collector->OnMainframeNavigatedTo(GURL("http://navigated.com"));
  }

  std::vector<UkmEntry> expected_entries{
      UkmEntry{kID,
               {
                   {"LinkClicked", 0},
                   {"LinkPosition", 0},
                   {"ResourceType", 1},
                   {"Status", 7},
               }},
      UkmEntry{kID,
               {
                   {"LinkClicked", 1},
                   {"LinkPosition", 1},
                   {"ResourceType", 1},
                   {"Status", 7},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest,
       SubresourcesNavigationUpdatedWithMainframe) {
  GURL mainframe_url("https://eligible.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnMainframeResourceNotEligible(
        GURL("http://not-eligible.com"),
        /*prediction_position=*/0,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);

    collector->OnMainframeResourcePrefetched(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));

    collector->OnSubresourcePrefetched(
        mainframe_url, GURL("http://subresource.com/"),
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(10)),
        MakeCompletionStatus(net::OK, 234, base::Milliseconds(16)));

    collector->OnMainframeNavigatedTo(mainframe_url);
  }

  std::vector<UkmEntry> expected_entries{
      // Mainframe
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 14},
               }},
      // Subresource
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(234)},
                   {"FetchDurationMS", 6},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 10},
                   {"ResourceType", 2},
                   {"Status", 14},
               }},
      // Not eligible mainframe
      UkmEntry{kID,
               {
                   {"LinkClicked", 0},
                   {"LinkPosition", 0},
                   {"ResourceType", 1},
                   {"Status", 7},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, Mainframe404ResponseCode) {
  GURL mainframe_url("https://eligible.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnMainframeResourcePrefetched(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 404 Not Found\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));
  }

  std::vector<UkmEntry> expected_entries{
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"LinkClicked", 0},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 12},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, DecoySuccess) {
  GURL mainframe_url("https://test.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    // Ensure that it is ok to report it as not eligible, then still send the
    // decoy.
    collector->OnMainframeResourceNotEligible(
        mainframe_url,
        /*prediction_position=*/0,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);

    collector->OnDecoyPrefetchComplete(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));

    collector->OnMainframeNavigatedTo(mainframe_url);
  }

  std::vector<UkmEntry> expected_entries{
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 29},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, NoResponseHead) {
  GURL mainframe_url("https://eligible.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnMainframeResourcePrefetched(
        mainframe_url,
        /*prediction_position=*/0, nullptr,
        MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));
  }

  std::vector<UkmEntry> expected_entries{
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"LinkClicked", 0},
                   {"LinkPosition", 0},
                   {"ResourceType", 1},
                   {"Status", 11},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, NetError) {
  GURL mainframe_url("https://eligible.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnMainframeResourcePrefetched(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::ERR_FAILED, 123, base::Milliseconds(10)));
  }

  std::vector<UkmEntry> expected_entries{
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"LinkClicked", 0},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 11},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, DecoyNetError) {
  GURL mainframe_url("https://test.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnDecoyPrefetchComplete(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::ERR_FAILED, 123, base::Milliseconds(10)));
  }

  std::vector<UkmEntry> expected_entries{
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"LinkClicked", 0},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 29},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, ProbeResult) {
  GURL mainframe_url("https://eligible.com");

  const struct {
    PrefetchProxyProbeResult probe_result;
    int want_status;
  } kTestCases[] = {
      {PrefetchProxyProbeResult::kNoProbing, 0},
      {PrefetchProxyProbeResult::kDNSProbeSuccess, 1},
      {PrefetchProxyProbeResult::kTLSProbeSuccess, 1},
      {PrefetchProxyProbeResult::kDNSProbeFailure, 2},
      {PrefetchProxyProbeResult::kTLSProbeFailure, 2},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(static_cast<int>(test.probe_result));
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    {
      auto collector =
          base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
              kNavigationStartTime, kID);

      collector->OnMainframeResourcePrefetched(
          mainframe_url,
          /*prediction_position=*/0,
          MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
          MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));

      collector->OnMainframeNavigatedTo(mainframe_url);

      collector->OnMainframeNavigationProbeResult(mainframe_url,
                                                  test.probe_result);
    }

    std::vector<UkmEntry> expected_entries{
        UkmEntry{
            kID,
            {
                {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                {"FetchDurationMS", 5},
                {"ISPFilteringStatus", static_cast<int>(test.probe_result)},
                {"LinkClicked", 1},
                {"LinkPosition", 0},
                {"NavigationStartToFetchStartMS", 5},
                {"ResourceType", 1},
                {"Status", test.want_status},
            }},
    };
    auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
    EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
        << ActualUkmEntriesToDebugString(entries);
  }
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, SubresourceReused) {
  GURL mainframe_url("https://eligible.com");
  GURL subresource_url("https://subresource.com");
  GURL subresource_url2("https://subresource.com/2");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnMainframeResourcePrefetched(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));

    collector->OnSubresourcePrefetched(
        mainframe_url, subresource_url,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(6)),
        MakeCompletionStatus(net::OK, 234, base::Milliseconds(12)));

    collector->OnSubresourcePrefetched(
        mainframe_url, subresource_url2,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(7)),
        MakeCompletionStatus(net::OK, 345, base::Milliseconds(14)));

    collector->OnMainframeNavigatedTo(mainframe_url);

    collector->OnMainframeNavigationProbeResult(
        mainframe_url, PrefetchProxyProbeResult::kNoProbing);

    collector->OnCachedSubresourceUsed(mainframe_url, subresource_url);
  }

  std::vector<UkmEntry> expected_entries{
      // Mainframe
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 0},
               }},
      // Reused subresource.
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(234)},
                   {"FetchDurationMS", 6},
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 6},
                   {"ResourceType", 2},
                   {"Status", 0},
               }},
      // Subresource, not reused.
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(345)},
                   {"FetchDurationMS", 7},
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 7},
                   {"ResourceType", 2},
                   {"Status", 14},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}

TEST_F(PrefetchProxyPrefetchMetricsCollectorTest, TypicalUsage) {
  GURL mainframe_url("https://eligible.com");
  GURL subresource_url("https://subresource.com");
  GURL subresource_url2("https://subresource.com/2");

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  {
    auto collector =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            kNavigationStartTime, kID);

    collector->OnMainframeResourcePrefetched(
        mainframe_url,
        /*prediction_position=*/0,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(5)),
        MakeCompletionStatus(net::OK, 123, base::Milliseconds(10)));

    collector->OnMainframeResourceNotEligible(
        GURL("http://ineligible.com"),
        /*prediction_position=*/1,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);

    collector->OnSubresourcePrefetched(
        mainframe_url, subresource_url,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(6)),
        MakeCompletionStatus(net::OK, 234, base::Milliseconds(12)));

    collector->OnSubresourcePrefetched(
        mainframe_url, subresource_url2,
        MakeHead("HTTP/1.1 200 OK\n", base::Milliseconds(7)),
        MakeCompletionStatus(net::OK, 345, base::Milliseconds(14)));

    collector->OnSubresourceNotEligible(
        mainframe_url, GURL("http://ineligible.com/subresource"),
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);

    collector->OnMainframeNavigatedTo(mainframe_url);

    collector->OnMainframeNavigationProbeResult(
        mainframe_url, PrefetchProxyProbeResult::kNoProbing);

    collector->OnCachedSubresourceUsed(mainframe_url, subresource_url);
  }

  std::vector<UkmEntry> expected_entries{
      // Prefetched mainframe.
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(123)},
                   {"FetchDurationMS", 5},
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 5},
                   {"ResourceType", 1},
                   {"Status", 0},
               }},
      // Ineligible mainframe.
      UkmEntry{kID,
               {
                   {"LinkClicked", 0},
                   {"LinkPosition", 1},
                   {"ResourceType", 1},
                   {"Status", 5},
               }},
      // Reused subresource.
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(234)},
                   {"FetchDurationMS", 6},
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 6},
                   {"ResourceType", 2},
                   {"Status", 0},
               }},
      // Subresource, not reused.
      UkmEntry{kID,
               {
                   {"DataLength", ukm::GetExponentialBucketMinForBytes(345)},
                   {"FetchDurationMS", 7},
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"NavigationStartToFetchStartMS", 7},
                   {"ResourceType", 2},
                   {"Status", 14},
               }},
      // Ineligible subresource.
      UkmEntry{kID,
               {
                   {"ISPFilteringStatus", 0},
                   {"LinkClicked", 1},
                   {"LinkPosition", 0},
                   {"ResourceType", 2},
                   {"Status", 5},
               }},
  };
  auto entries = ukm_recorder.GetEntries(kEventName, kAllEventMetrics);
  EXPECT_THAT(entries, testing::UnorderedElementsAreArray(expected_entries))
      << ActualUkmEntriesToDebugString(entries);
}
