// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/authenticated_network_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/update_client/network.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Return;

namespace extensions {

namespace {

class MockNetworkFetcher : public update_client::NetworkFetcher {
 public:
  MockNetworkFetcher() = default;
  ~MockNetworkFetcher() override = default;

  MOCK_METHOD(void,
              PostRequest,
              (const GURL& url,
               const std::string& post_data,
               const std::string& content_type,
               (const base::flat_map<std::string, std::string>&),
               ResponseStartedCallback,
               ProgressCallback,
               PostRequestCompleteCallback),
              (override));

  MOCK_METHOD(base::OnceClosure,
              DownloadToFile,
              (const GURL& url,
               const base::FilePath& file_path,
               ResponseStartedCallback,
               ProgressCallback,
               DownloadToFileCompleteCallback),
              (override));
};

// A NetworkFetcherFactory which holds a singular MockNetworkFetcher which can
// be configured by tests. Each call to `Create` returns a proxy to the mock
// object retained by the factory.
class MockNetworkFetcherFactory final
    : public update_client::NetworkFetcherFactory {
 public:
  MockNetworkFetcher& fetcher() LIFETIME_BOUND { return fetcher_; }

  std::unique_ptr<update_client::NetworkFetcher> Create() const override {
    return std::make_unique<Proxy>(&fetcher_);
  }

 private:
  class Proxy final : public update_client::NetworkFetcher {
   public:
    explicit Proxy(NetworkFetcher* fetcher) : fetcher_(fetcher) {}
    void PostRequest(
        const GURL& url,
        const std::string& post_data,
        const std::string& content_type,
        const base::flat_map<std::string, std::string>& post_additional_headers,
        ResponseStartedCallback response_started_callback,
        ProgressCallback progress_callback,
        PostRequestCompleteCallback post_request_complete_callback) override {
      fetcher_->PostRequest(url, post_data, content_type,
                            post_additional_headers, response_started_callback,
                            progress_callback,
                            std::move(post_request_complete_callback));
    }

    base::OnceClosure DownloadToFile(
        const GURL& url,
        const base::FilePath& file_path,
        ResponseStartedCallback response_started_callback,
        ProgressCallback progress_callback,
        DownloadToFileCompleteCallback download_to_file_complete_callback)
        override {
      return fetcher_->DownloadToFile(
          url, file_path, response_started_callback, progress_callback,
          std::move(download_to_file_complete_callback));
    }

   private:
    const raw_ptr<update_client::NetworkFetcher> fetcher_;
  };

  ~MockNetworkFetcherFactory() override = default;

  mutable MockNetworkFetcher fetcher_;
};

}  // namespace

class AuthenticatedNetworkFetcherTest : public testing::Test {
 protected:
  MockNetworkFetcher& mock_fetcher() LIFETIME_BOUND {
    return mock_fetcher_factory_->fetcher();
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockNetworkFetcherFactory> mock_fetcher_factory_ =
      base::MakeRefCounted<MockNetworkFetcherFactory>();
  AuthenticatedNetworkFetcher authenticated_fetcher_{mock_fetcher_factory_};
};

TEST_F(AuthenticatedNetworkFetcherTest, PostRequest_Success) {
  GURL url("https://chrome.google.com/webstore");
  EXPECT_CALL(mock_fetcher(), PostRequest(url, _, _, _, _, _, _))
      .WillOnce([](const GURL&, const std::string&, const std::string&,
                   const base::flat_map<std::string, std::string>&,
                   update_client::NetworkFetcher::ResponseStartedCallback
                       response_started,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::PostRequestCompleteCallback
                       complete) {
        response_started.Run(net::HTTP_OK, 100);
        std::move(complete).Run("response", 0, "", "", "", -1);
      });

  base::RunLoop run_loop;
  base::MockCallback<update_client::NetworkFetcher::ResponseStartedCallback>
      response_started_callback;
  base::MockCallback<update_client::NetworkFetcher::PostRequestCompleteCallback>
      post_request_complete_callback;

  EXPECT_CALL(response_started_callback, Run(net::HTTP_OK, 100));
  EXPECT_CALL(post_request_complete_callback,
              Run(std::optional<std::string>("response"), 0, _, _, _, _))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  authenticated_fetcher_.PostRequest(url, "data", "text/plain", {},
                                     response_started_callback.Get(),
                                     /*progress_callback=*/base::DoNothing(),
                                     post_request_complete_callback.Get());
  run_loop.Run();
}

class AuthenticatedNetworkFetcherAuthErrorTest
    : public AuthenticatedNetworkFetcherTest,
      public testing::WithParamInterface<net::HttpStatusCode> {};

TEST_P(AuthenticatedNetworkFetcherAuthErrorTest,
       PostRequest_NoRetryOnAuthError) {
  GURL url("https://chrome.google.com/webstore");
  const net::HttpStatusCode auth_error = GetParam();

  EXPECT_CALL(mock_fetcher(), PostRequest(url, _, _, _, _, _, _))
      .WillOnce([auth_error](
                    const GURL&, const std::string&, const std::string&,
                    const base::flat_map<std::string, std::string>&,
                    update_client::NetworkFetcher::ResponseStartedCallback
                        response_started,
                    update_client::NetworkFetcher::ProgressCallback,
                    update_client::NetworkFetcher::PostRequestCompleteCallback
                        complete) {
        response_started.Run(auth_error, 0);
        std::move(complete).Run(std::nullopt, 0, "", "", "", -1);
      });

  base::RunLoop run_loop;
  base::MockCallback<update_client::NetworkFetcher::ResponseStartedCallback>
      response_started_callback;
  base::MockCallback<update_client::NetworkFetcher::PostRequestCompleteCallback>
      post_request_complete_callback;

  EXPECT_CALL(response_started_callback, Run(auth_error, 0));
  EXPECT_CALL(post_request_complete_callback,
              Run(std::optional<std::string>(), 0, _, _, _, _))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  authenticated_fetcher_.PostRequest(url, "data", "text/plain", {},
                                     response_started_callback.Get(),
                                     /*progress_callback=*/base::DoNothing(),
                                     post_request_complete_callback.Get());
  run_loop.Run();
}

TEST_P(AuthenticatedNetworkFetcherAuthErrorTest,
       DownloadToFile_RetryOnAuthError) {
  GURL url("https://chrome.google.com/download");
  GURL retried_url("https://chrome.google.com/download?authuser=1");
  base::FilePath path(FILE_PATH_LITERAL("test.crx"));
  const net::HttpStatusCode auth_error = GetParam();

  EXPECT_CALL(mock_fetcher(), DownloadToFile(url, path, _, _, _))
      .WillOnce(
          [auth_error](
              const GURL&, const base::FilePath&,
              update_client::NetworkFetcher::ResponseStartedCallback
                  response_started,
              update_client::NetworkFetcher::ProgressCallback,
              update_client::NetworkFetcher::DownloadToFileCompleteCallback
                  complete) {
            response_started.Run(auth_error, 0);
            std::move(complete).Run(0, 0);
            return base::OnceClosure(base::DoNothing());
          });

  EXPECT_CALL(mock_fetcher(), DownloadToFile(retried_url, path, _, _, _))
      .WillOnce([](const GURL&, const base::FilePath&,
                   update_client::NetworkFetcher::ResponseStartedCallback
                       response_started,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::DownloadToFileCompleteCallback
                       complete) {
        response_started.Run(net::HTTP_OK, 1000);
        std::move(complete).Run(0, 1000);
        return base::OnceClosure(base::DoNothing());
      });

  base::RunLoop run_loop;
  base::MockCallback<update_client::NetworkFetcher::ResponseStartedCallback>
      response_started_callback;
  base::MockCallback<
      update_client::NetworkFetcher::DownloadToFileCompleteCallback>
      download_to_file_complete_callback;

  EXPECT_CALL(response_started_callback, Run(net::HTTP_OK, 1000));
  EXPECT_CALL(download_to_file_complete_callback, Run(0, 1000))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  authenticated_fetcher_.DownloadToFile(
      url, path, response_started_callback.Get(),
      /*progress_callback=*/base::DoNothing(),
      download_to_file_complete_callback.Get());
  run_loop.Run();
}

TEST_P(AuthenticatedNetworkFetcherAuthErrorTest,
       DownloadToFile_PreservesQueryParameters) {
  GURL url("https://google.com/download?other=param");
  GURL retried_url("https://google.com/download?other=param&authuser=1");
  base::FilePath path(FILE_PATH_LITERAL("test.crx"));
  const net::HttpStatusCode auth_error = GetParam();

  EXPECT_CALL(mock_fetcher(), DownloadToFile(url, path, _, _, _))
      .WillOnce(
          [auth_error](
              const GURL&, const base::FilePath&,
              update_client::NetworkFetcher::ResponseStartedCallback
                  response_started,
              update_client::NetworkFetcher::ProgressCallback,
              update_client::NetworkFetcher::DownloadToFileCompleteCallback
                  complete) {
            response_started.Run(auth_error, 0);
            std::move(complete).Run(0, 0);
            return base::OnceClosure(base::DoNothing());
          });

  EXPECT_CALL(mock_fetcher(), DownloadToFile(retried_url, path, _, _, _))
      .WillOnce([](const GURL&, const base::FilePath&,
                   update_client::NetworkFetcher::ResponseStartedCallback
                       response_started,
                   update_client::NetworkFetcher::ProgressCallback,
                   update_client::NetworkFetcher::DownloadToFileCompleteCallback
                       complete) {
        response_started.Run(net::HTTP_OK, 1000);
        std::move(complete).Run(0, 1000);
        return base::OnceClosure(base::DoNothing());
      });

  base::RunLoop run_loop;
  base::MockCallback<
      update_client::NetworkFetcher::DownloadToFileCompleteCallback>
      download_to_file_complete_callback;

  EXPECT_CALL(download_to_file_complete_callback, Run(0, 1000))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  authenticated_fetcher_.DownloadToFile(
      url, path, /*response_started_callback=*/base::DoNothing(),
      /*progress_callback=*/base::DoNothing(),
      download_to_file_complete_callback.Get());
  run_loop.Run();
}

TEST_P(AuthenticatedNetworkFetcherAuthErrorTest, DownloadToFile_RetryLimit) {
  const net::HttpStatusCode auth_error = GetParam();
  const int kMaxAuthUserValue = 10;
  base::FilePath path(FILE_PATH_LITERAL("test.crx"));

  for (int i = 0; i <= kMaxAuthUserValue; ++i) {
    GURL url = i == 0
                   ? GURL("https://google.com/download")
                   : GURL(base::StrCat({"https://google.com/download?authuser=",
                                        base::NumberToString(i)}));
    EXPECT_CALL(mock_fetcher(), DownloadToFile(url, path, _, _, _))
        .WillOnce(
            [auth_error](
                const GURL&, const base::FilePath&,
                update_client::NetworkFetcher::ResponseStartedCallback
                    response_started,
                update_client::NetworkFetcher::ProgressCallback,
                update_client::NetworkFetcher::DownloadToFileCompleteCallback
                    complete) {
              response_started.Run(auth_error, 0);
              std::move(complete).Run(0, 0);
              return base::OnceClosure(base::DoNothing());
            });
  }

  base::RunLoop run_loop;
  base::MockCallback<update_client::NetworkFetcher::ResponseStartedCallback>
      response_started_callback;
  base::MockCallback<
      update_client::NetworkFetcher::DownloadToFileCompleteCallback>
      download_to_file_complete_callback;

  EXPECT_CALL(response_started_callback, Run(auth_error, 0));
  EXPECT_CALL(download_to_file_complete_callback, Run(0, 0))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  authenticated_fetcher_.DownloadToFile(
      GURL("https://google.com/download"), path,
      response_started_callback.Get(), /*progress_callback=*/base::DoNothing(),
      download_to_file_complete_callback.Get());
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         AuthenticatedNetworkFetcherAuthErrorTest,
                         testing::Values(net::HTTP_UNAUTHORIZED,
                                         net::HTTP_FORBIDDEN));

}  // namespace extensions
