// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_connection_info.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
namespace {

// Used for the main frame, in subresource tests.
constexpr char kUninterestingMainFramePath[] =
    "/uninteresting/main_frame/path/";

// Used for the "interesting" request individual tests focus on.
constexpr char kInterestingPath[] = "/interesting/path/";

enum class RequestType {
  kMainFrame,
  kSubFrame,
  kImage,
  kScript,
};

enum class HeadersReceived {
  kHeadersReceived,
  kNoHeadersReceived,
};

enum class NetworkAccessed {
  kNetworkAccessed,
  kNoNetworkAccessed,
};

// Utility class to wait until the main resource load is complete. This is to
// make sure, in the cancel tests, the main resource is fully loaded before the
// navigation is cancelled, to ensure the main frame load histograms are in a
// consistent state, and can be checked at the end of each test. To avoid races,
// create this class before starting a navigation.
class WaitForMainFrameResourceObserver : public content::WebContentsObserver {
 public:
  explicit WaitForMainFrameResourceObserver(WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  WaitForMainFrameResourceObserver(const WaitForMainFrameResourceObserver&) =
      delete;
  WaitForMainFrameResourceObserver& operator=(
      const WaitForMainFrameResourceObserver&) = delete;

  ~WaitForMainFrameResourceObserver() override {}

  // content::WebContentsObserver implementation:
  void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override {
    EXPECT_EQ(network::mojom::RequestDestination::kDocument,
              resource_load_info.request_destination);
    EXPECT_EQ(net::OK, resource_load_info.net_error);
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

// This test fixture tests code in content/. The fixture itself is in chrome/
// because SubprocessMetricsProvider is a chrome-only test class.
class NetworkRequestMetricsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<RequestType> {
 public:
  ~NetworkRequestMetricsBrowserTest() override = default;

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    uninteresting_main_frame_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kUninterestingMainFramePath);
    interesting_http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kInterestingPath);
    ASSERT_TRUE(embedded_test_server()->Start());

    // Need to make this after test setup, to make sure the initial about:blank
    // load is not counted.
    histograms_ = std::make_unique<base::HistogramTester>();
  }

  // For non-RequestType::kMainFrame tests, returns the contents of the main
  // frame, based on the RequestType.
  std::string GetMainFrameContents(const std::string subresource_path) {
    switch (GetParam()) {
      case RequestType::kSubFrame:
        return base::StringPrintf("<iframe src='%s'></iframe>",
                                  subresource_path.c_str());
      case RequestType::kImage:
        return base::StringPrintf("<img src='%s'>", subresource_path.c_str());
      case RequestType::kScript:
        return base::StringPrintf("<script src='%s'></script>",
                                  subresource_path.c_str());
      case RequestType::kMainFrame:
        NOTREACHED_IN_MIGRATION();
    }
    return std::string();
  }

  void StartNavigatingAndWaitForRequest() {
    GURL interesting_url = embedded_test_server()->GetURL(kInterestingPath);
    if (GetParam() == RequestType::kMainFrame) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), interesting_url, WindowOpenDisposition::CURRENT_TAB,
          ui_test_utils::BROWSER_TEST_NO_WAIT);
    } else {
      WaitForMainFrameResourceObserver wait_for_main_frame_resource_observer(
          active_web_contents());
      ui_test_utils::NavigateToURLWithDisposition(
          browser(),
          embedded_test_server()->GetURL(kUninterestingMainFramePath),
          WindowOpenDisposition::CURRENT_TAB,
          ui_test_utils::BROWSER_TEST_NO_WAIT);
      uninteresting_main_frame_response_->WaitForRequest();
      uninteresting_main_frame_response_->Send(
          "HTTP/1.1 200 Peachy\r\n"
          "Content-Type: text/html\r\n"
          "\r\n");
      uninteresting_main_frame_response_->Send(
          GetMainFrameContents(interesting_url.spec()));
      uninteresting_main_frame_response_->Done();
      wait_for_main_frame_resource_observer.Wait();
    }

    interesting_http_response_->WaitForRequest();
  }

  net::test_server::ControllableHttpResponse* interesting_http_response() {
    return interesting_http_response_.get();
  }

  // Checks all relevant histograms. |expected_net_error| is the expected result
  // of the RequestType specified by the test parameter.
  void CheckHistograms(int expected_net_error,
                       HeadersReceived headers_received,
                       NetworkAccessed network_accessed) {
    // Some metrics may come from the renderer. This call ensures that those
    // metrics are available.
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    if (GetParam() == RequestType::kMainFrame) {
      histograms_->ExpectUniqueSample("Net.ErrorCodesForMainFrame4",
                                      -expected_net_error, 1);

      if (headers_received == HeadersReceived::kHeadersReceived) {
        histograms_->ExpectUniqueSample("Net.ConnectionInfo.MainFrame",
                                        net::HttpConnectionInfo::kHTTP1_1, 1);
      } else {
        histograms_->ExpectTotalCount("Net.ConnectionInfo.MainFrame", 0);
      }

      // Favicon may or may not have been loaded.
      EXPECT_GE(
          1u,
          histograms_->GetAllSamples("Net.ErrorCodesForSubresources3").size());
      EXPECT_GE(
          1u,
          histograms_->GetAllSamples("Net.ConnectionInfo.SubResource").size());

      return;
    }

    // If not testing the main frame, there should also be just one result for
    // the main frame.
    histograms_->ExpectUniqueSample("Net.ErrorCodesForMainFrame4", -net::OK, 1);

    // Some fuzziness here because of the favicon. It should typically succeed,
    // but allow it to have been aborted, too, since the test server won't
    // return a valid icon.
    std::vector<base::Bucket> buckets =
        histograms_->GetAllSamples("Net.ErrorCodesForSubresources3");
    bool found_expected_load = false;
    bool found_favicon_load = false;
    for (auto& bucket : buckets) {
      if (!found_expected_load && bucket.min == -expected_net_error) {
        found_expected_load = true;
        bucket.count--;
      }
      if (!found_favicon_load && bucket.count > 0 &&
          (bucket.min == -net::OK || bucket.min == -net::ERR_ABORTED)) {
        found_favicon_load = true;
        bucket.count--;
      }
      EXPECT_EQ(0, bucket.count)
          << "Found unexpected load with result: " << bucket.min;
    }
    EXPECT_TRUE(found_expected_load);

    // A subresource load requires a main frame load, which is only logged for
    // network URLs.
    if (network_accessed == NetworkAccessed::kNetworkAccessed) {
      histograms_->ExpectUniqueSample("Net.ConnectionInfo.MainFrame",
                                      net::HttpConnectionInfo::kHTTP1_1, 1);
      if (headers_received == HeadersReceived::kHeadersReceived) {
        // Favicon request may or may not have received a response.
        size_t subresources =
            histograms_->GetAllSamples("Net.ConnectionInfo.SubResource").size();
        EXPECT_LE(1u, subresources);
        EXPECT_GE(2u, subresources);
      } else {
        histograms_->ExpectTotalCount("Net.ConnectionInfo.SubResource", 0);
      }
    } else {
      histograms_->ExpectTotalCount("Net.ConnectionInfo.MainFrame", 0);
      histograms_->ExpectTotalCount("Net.ConnectionInfo.SubResource", 0);
    }
  }

  // Checks all relevant histograms in the case the navigation is interrupted.
  // The request identified by GetParam() is expected to fail with
  // net::ERR_ABORTED.
  void CheckHistogramsAfterMainFrameInterruption() {
    // Some metrics may come from the renderer. These call ensures that those
    // metrics are available.
    FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    if (GetParam() == RequestType::kMainFrame) {
      // Can't check Net.ErrorCodesForSubresources3, due to the favicon, which
      // Chrome may or may not have attempted to load.
      histograms_->ExpectTotalCount("Net.ErrorCodesForMainFrame4", 1);
      EXPECT_EQ(1, histograms_->GetBucketCount("Net.ErrorCodesForMainFrame4",
                                               -net::ERR_ABORTED));
      return;
    }

    histograms_->ExpectUniqueSample("Net.ErrorCodesForMainFrame4", -net::OK, 1);

    // Some fuzziness here because of the favicon. It should typically succeed,
    // but allow it to have been aborted, too, since the test server won't
    // return a valid icon.
    std::vector<base::Bucket> buckets =
        histograms_->GetAllSamples("Net.ErrorCodesForSubresources3");
    bool found_expected_load = false;
    int found_favicon_loads = 0;
    for (auto& bucket : buckets) {
      if (!found_expected_load && bucket.min == -net::ERR_ABORTED) {
        found_expected_load = true;
        bucket.count--;
      }
      // Allow up to two favicon loads, one for the original page load, that was
      // interrupted by a new load, and one for the new page load.
      if (found_favicon_loads < 2 && bucket.count > 0 &&
          (bucket.min == -net::OK || bucket.min == -net::ERR_ABORTED)) {
        found_favicon_loads++;
        bucket.count--;
      }
      EXPECT_EQ(0, bucket.count)
          << "Found unexpected load with result: " << bucket.min;
    }
    EXPECT_TRUE(found_expected_load);
  }

  // Send headers and a partial body to |interesting_http_response_|. Doesn't
  // terminate the response, so the socket can either be closed, or the request
  // aborted.
  void SendHeadersPartialBody() {
    // Sending a body that's too short will result in an error after all the
    // bytes are read.
    interesting_http_response_->Send("HTTP/1.1 200 Peachy\r\n");
    // Send a MIME type to avoid MIME sniffing (Shouldn't matter, but it's one
    // less thing to worry about).
    if (GetParam() == RequestType::kImage) {
      interesting_http_response_->Send("Content-Type: image/png\r\n");
    } else if (GetParam() == RequestType::kScript) {
      interesting_http_response_->Send("Content-Type: text/css\r\n");
    } else {
      interesting_http_response_->Send("Content-Type: text/html\r\n");
    }
    interesting_http_response_->Send("Content-Length: 50\r\n\r\n");
    // This is the first byte of the PNG header, to avoid any chance of the
    // request being cancelled for not looking like a PNG.
    interesting_http_response_->Send("\x89");
  }

  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::HistogramTester* histograms() { return histograms_.get(); }

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      uninteresting_main_frame_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      interesting_http_response_;
  std::unique_ptr<base::HistogramTester> histograms_;
};

// Testing before headers / during body is most interesting in the frame case,
// as it checks the before and after commit case, which follow very different
// paths.
IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest,
                       NetErrorBeforeHeaders) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  interesting_http_response()->Send(
      "HTTP/1.1 200 OK\r\nContent-Length: 42\r\nContent-Length: 43\r\n\r\n");
  interesting_http_response()->Done();
  navigation_observer.Wait();

  CheckHistograms(net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH,
                  HeadersReceived::kNoHeadersReceived,
                  NetworkAccessed::kNetworkAccessed);
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, NetErrorDuringBody) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  SendHeadersPartialBody();
  interesting_http_response()->Done();
  navigation_observer.Wait();

  CheckHistograms(net::ERR_CONTENT_LENGTH_MISMATCH,
                  HeadersReceived::kHeadersReceived,
                  NetworkAccessed::kNetworkAccessed);
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, CancelBeforeHeaders) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  active_web_contents()->Stop();
  navigation_observer.Wait();

  CheckHistograms(net::ERR_ABORTED, HeadersReceived::kNoHeadersReceived,
                  NetworkAccessed::kNetworkAccessed);
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, CancelDuringBody) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  SendHeadersPartialBody();

  // Unfortunately, there's no way to ensure that the body has partially been
  // received, so can only wait and hope. If the partial body hasn't been
  // recieved by the time Stop() is called, the test should still pass, however.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  run_loop.Run();

  active_web_contents()->Stop();
  navigation_observer.Wait();

  CheckHistograms(net::ERR_ABORTED, HeadersReceived::kHeadersReceived,
                  NetworkAccessed::kNetworkAccessed);
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest,
                       InterruptedBeforeHeaders) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();

  // Stop navigation to record histograms.
  active_web_contents()->Stop();
  navigation_observer.Wait();

  CheckHistogramsAfterMainFrameInterruption();
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest,
                       InterruptedCancelDuringBody) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  SendHeadersPartialBody();

  // Unfortunately, there's no way to ensure that the body has partially been
  // received, so can only wait and hope. If the partial body hasn't been
  // recieved by the time Stop() is called, the test should still pass, however.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  run_loop.Run();

  // Stop navigation to record histograms.
  active_web_contents()->Stop();
  navigation_observer.Wait();

  CheckHistogramsAfterMainFrameInterruption();
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, SuccessWithBody) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  interesting_http_response()->Send("HTTP/1.1 200 Peachy\r\n\r\n");
  interesting_http_response()->Send("Grapefruit");
  interesting_http_response()->Done();
  navigation_observer.Wait();

  CheckHistograms(net::OK, HeadersReceived::kHeadersReceived,
                  NetworkAccessed::kNetworkAccessed);
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, SuccessWithEmptyBody) {
  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  interesting_http_response()->Send("HTTP/1.1 200 Peachy\r\n");
  interesting_http_response()->Send("Content-Length: 0\r\n\r\n");
  interesting_http_response()->Done();
  navigation_observer.Wait();

  CheckHistograms(net::OK, HeadersReceived::kHeadersReceived,
                  NetworkAccessed::kNetworkAccessed);
}

// Downloads should not be logged (Either as successes or failures).
IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, Download) {
  // Only frames can be converted to downloads.
  if (GetParam() != RequestType::kMainFrame &&
      GetParam() != RequestType::kSubFrame) {
    return;
  }

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDownloadRestrictions,
      static_cast<int>(DownloadPrefs::DownloadRestriction::ALL_FILES));
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);

  // Need this to wait for the download to be fully cancelled to avoid a
  // confirmation prompt on quit.
  DownloadTestObserverTerminal download_test_observer_terminal(
      browser()->profile()->GetDownloadManager(), 1,
      DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  TestNavigationObserver navigation_observer(active_web_contents(), 1);
  StartNavigatingAndWaitForRequest();
  interesting_http_response()->Send("HTTP/1.1 200 Peachy\r\n");
  interesting_http_response()->Send(
      "Content-Type: binary/octet-stream\r\n\r\n");
  interesting_http_response()->Send("\x01");
  interesting_http_response()->Done();
  navigation_observer.Wait();

  download_test_observer_terminal.WaitForFinished();

  // Some metrics may come from the renderer. This call ensures that those
  // metrics are available.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  if (GetParam() == RequestType::kMainFrame) {
    histograms()->ExpectTotalCount("Net.ErrorCodesForMainFrame4", 0);
    histograms()->ExpectTotalCount("Net.ConnectionInfo.MainFrame", 0);
    // Favicon may or may not have been loaded.
    EXPECT_GE(
        1u,
        histograms()->GetAllSamples("Net.ErrorCodesForSubresources3").size());
    EXPECT_GE(
        1u,
        histograms()->GetAllSamples("Net.ConnectionInfo.SubResource").size());

    return;
  }

  // If not testing the main frame, there should also be just one result for
  // the main frame.
  histograms()->ExpectUniqueSample("Net.ErrorCodesForMainFrame4", -net::OK, 1);

  // Some fuzziness here because of the favicon. It should typically succeed,
  // but allow it to have been aborted, too, since the test server won't
  // return a valid icon.
  std::vector<base::Bucket> buckets =
      histograms()->GetAllSamples("Net.ErrorCodesForSubresources3");
  bool found_favicon_load = false;
  for (auto& bucket : buckets) {
    if (!found_favicon_load && bucket.count > 0 &&
        (bucket.min == -net::OK || bucket.min == -net::ERR_ABORTED)) {
      found_favicon_load = true;
      bucket.count--;
    }
    EXPECT_EQ(0, bucket.count)
        << "Found unexpected load with result: " << bucket.min;
  }

  histograms()->ExpectUniqueSample("Net.ConnectionInfo.MainFrame",
                                   net::HttpConnectionInfo::kHTTP1_1, 1);
  // Favicon request may or may not have received a response.
  size_t subresources =
      histograms()->GetAllSamples("Net.ConnectionInfo.SubResource").size();
  EXPECT_LE(0u, subresources);
  EXPECT_GE(1u, subresources);
}

// A few tests for file:// URLs, so that URLs not handled by the network service
// itself have some coverage.

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, FileURLError) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::FilePath main_frame_path = temp_dir_.GetPath().AppendASCII("main.html");
  if (GetParam() != RequestType::kMainFrame) {
    std::string main_frame_data = GetMainFrameContents("subresource");
    ASSERT_TRUE(base::WriteFile(main_frame_path, main_frame_data));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), net::FilePathToFileURL(main_frame_path)));
  CheckHistograms(net::ERR_FILE_NOT_FOUND, HeadersReceived::kNoHeadersReceived,
                  NetworkAccessed::kNoNetworkAccessed);
}

IN_PROC_BROWSER_TEST_P(NetworkRequestMetricsBrowserTest, FileURLSuccess) {
  const char kSubresourcePath[] = "subresource";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::FilePath main_frame_path = temp_dir_.GetPath().AppendASCII("main.html");
  std::string main_frame_data = "foo";
  if (GetParam() != RequestType::kMainFrame)
    main_frame_data = GetMainFrameContents(kSubresourcePath);
  ASSERT_TRUE(base::WriteFile(main_frame_path, main_frame_data));
  if (GetParam() != RequestType::kMainFrame) {
    std::string subresource_data = "foo";
    ASSERT_TRUE(base::WriteFile(
        temp_dir_.GetPath().AppendASCII(kSubresourcePath), subresource_data));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), net::FilePathToFileURL(main_frame_path)));
  CheckHistograms(net::OK, HeadersReceived::kNoHeadersReceived,
                  NetworkAccessed::kNoNetworkAccessed);
}

INSTANTIATE_TEST_SUITE_P(All,
                         NetworkRequestMetricsBrowserTest,
                         testing::Values(RequestType::kMainFrame,
                                         RequestType::kSubFrame,
                                         RequestType::kImage,
                                         RequestType::kScript));

}  //  namespace
}  // namespace content
