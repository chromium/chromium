// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_report_sender.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A fake SharedURLLoaderFactory that always returns 200 OK.
class FakeSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FakeSharedURLLoaderFactory() = default;

  const std::vector<network::ResourceRequest> resource_requests() {
    return resource_requests_;
  }

 private:
  ~FakeSharedURLLoaderFactory() override = default;

  // network::SharedURLLoaderFactory
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    resource_requests_.push_back(url_request);
    mojo::Remote<network::mojom::URLLoaderClient> client_remote(
        std::move(client));
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\n\n");
    head->mime_type = "text/html";
    client_remote->OnReceiveResponse(
        std::move(head), mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
    client_remote->OnComplete(network::URLLoaderCompletionStatus());
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  std::vector<network::ResourceRequest> resource_requests_;
};

}  // namespace

TEST(ChromeReportSenderTest, DoesNotSaveOrSendCookies) {
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<FakeSharedURLLoaderFactory> test_loader_factory =
      base::MakeRefCounted<FakeSharedURLLoaderFactory>();
  GURL report_url("https://example.com/");

  base::RunLoop run_loop;
  SendReport(test_loader_factory, TRAFFIC_ANNOTATION_FOR_TESTS, report_url,
             "application/octet-stream", "report contents",
             run_loop.QuitClosure(), base::DoNothing());
  run_loop.Run();

  ASSERT_EQ(test_loader_factory->resource_requests().size(), 1u);
  EXPECT_EQ(test_loader_factory->resource_requests()[0].credentials_mode,
            network::mojom::CredentialsMode::kOmit);
}

TEST(ChromeReportSenderTest, UploadsToUrl) {
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<FakeSharedURLLoaderFactory> test_loader_factory =
      base::MakeRefCounted<FakeSharedURLLoaderFactory>();
  GURL report_url("https://example.com/");

  base::RunLoop run_loop;
  SendReport(test_loader_factory, TRAFFIC_ANNOTATION_FOR_TESTS, report_url,
             "application/octet-stream", "report contents",
             run_loop.QuitClosure(), base::DoNothing());
  run_loop.Run();

  ASSERT_EQ(test_loader_factory->resource_requests().size(), 1u);
  EXPECT_EQ(test_loader_factory->resource_requests()[0].url, report_url);
}

TEST(ChromeReportSenderTest, UsesPostMethod) {
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<FakeSharedURLLoaderFactory> test_loader_factory =
      base::MakeRefCounted<FakeSharedURLLoaderFactory>();
  GURL report_url("https://example.com/");

  base::RunLoop run_loop;
  SendReport(test_loader_factory, TRAFFIC_ANNOTATION_FOR_TESTS, report_url,
             "application/octet-stream", "report contents",
             run_loop.QuitClosure(), base::DoNothing());
  run_loop.Run();

  ASSERT_EQ(test_loader_factory->resource_requests().size(), 1u);
  EXPECT_EQ(test_loader_factory->resource_requests()[0].method,
            net::HttpRequestHeaders::kPostMethod);
}

TEST(ChromeReportSenderTest, SkipsCache) {
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<FakeSharedURLLoaderFactory> test_loader_factory =
      base::MakeRefCounted<FakeSharedURLLoaderFactory>();
  GURL report_url("https://example.com/");

  base::RunLoop run_loop;
  SendReport(test_loader_factory, TRAFFIC_ANNOTATION_FOR_TESTS, report_url,
             "application/octet-stream", "report contents",
             run_loop.QuitClosure(), base::DoNothing());
  run_loop.Run();

  ASSERT_EQ(test_loader_factory->resource_requests().size(), 1u);
  EXPECT_EQ(test_loader_factory->resource_requests()[0].load_flags,
            net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE);
}
