// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/background_loader_offliner.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

// A custom request handler that returns a response configured as a file
// download.
std::unique_ptr<net::test_server::HttpResponse> HandleDownloadRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/download.pdf") {
    return nullptr;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  // Set headers that typically trigger a download in Chromium.
  response->AddCustomHeader("Content-Type", "application/pdf");
  response->AddCustomHeader("Content-Disposition",
                            "attachment; filename=\"test.pdf\"");
  response->set_content("Dummy PDF content");
  return response;
}

class BackgroundLoaderOfflinerBrowserTest : public PlatformBrowserTest {
 public:
  BackgroundLoaderOfflinerBrowserTest() = default;
  ~BackgroundLoaderOfflinerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleDownloadRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Flush pending tasks to avoid use-after-free during test teardown.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<BackgroundLoaderOffliner> CreateOffliner() {
    // Obtain dependencies from the current browser profile.
    Profile* profile = chrome_test_utils::GetProfile(this);
    OfflinePageModel* model =
        OfflinePageModelFactory::GetForBrowserContext(profile);

    // We can use a default OfflinerPolicy.
    policy_ = std::make_unique<OfflinerPolicy>();

    return std::make_unique<BackgroundLoaderOffliner>(
        profile, policy_.get(), model, nullptr /* load_termination_listener */);
  }

 private:
  std::unique_ptr<OfflinerPolicy> policy_;
};

}  // namespace

// Tests that when a request belongs to a namespace that allows background file
// downloads (e.g. kBrowserActionsNamespace), the offliner correctly processes
// the download and finishes with DOWNLOAD_THROTTLED status.
IN_PROC_BROWSER_TEST_F(BackgroundLoaderOfflinerBrowserTest,
                       DownloadConversionAllowed) {
  auto offliner = CreateOffliner();

  // Create a request with a namespace that allows file downloads.
  ClientId client_id;
  client_id.name_space = kBrowserActionsNamespace;
  client_id.id = "test_id_allowed";

  SavePageRequest request(
      12345 /* request_id */, embedded_test_server()->GetURL("/download.pdf"),
      client_id, base::Time::Now(), true /* user_requested */);

  base::test::TestFuture<const SavePageRequest&, Offliner::RequestStatus>
      test_future;

  // Attempt to load and save the page. The response is an attachment, so it
  // should trigger CanDownload() during navigation.
  bool started = offliner->LoadAndSave(
      request, test_future.GetCallback(),
      base::BindRepeating([](const SavePageRequest& req, int64_t bytes) {}));

  ASSERT_TRUE(started);

  // The request should be allowed to convert to a download and fail the
  // background saving process with DOWNLOAD_THROTTLED.
  EXPECT_EQ(std::to_underlying(Offliner::RequestStatus::DOWNLOAD_THROTTLED),
            std::to_underlying(test_future.Get<Offliner::RequestStatus>()));
}

// Tests that when a request belongs to a namespace that does NOT allow
// background file downloads, the offliner blocks the download and fails
// with LOADING_FAILED_DOWNLOAD status.
IN_PROC_BROWSER_TEST_F(BackgroundLoaderOfflinerBrowserTest,
                       DownloadConversionBlocked) {
  auto offliner = CreateOffliner();

  // Create a request with a namespace that does NOT allow file downloads.
  ClientId client_id;
  client_id.name_space = kLastNNamespace;
  client_id.id = "test_id_blocked";

  SavePageRequest request(
      12346 /* request_id */, embedded_test_server()->GetURL("/download.pdf"),
      client_id, base::Time::Now(), true /* user_requested */);

  base::test::TestFuture<const SavePageRequest&, Offliner::RequestStatus>
      test_future;

  // Attempt to load and save the page.
  bool started = offliner->LoadAndSave(
      request, test_future.GetCallback(),
      base::BindRepeating([](const SavePageRequest& req, int64_t bytes) {}));

  ASSERT_TRUE(started);

  // The request should be blocked from converting to a download and fail the
  // background saving process with LOADING_FAILED_DOWNLOAD.
  EXPECT_EQ(
      std::to_underlying(Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD),
      std::to_underlying(test_future.Get<Offliner::RequestStatus>()));
}

}  // namespace offline_pages
