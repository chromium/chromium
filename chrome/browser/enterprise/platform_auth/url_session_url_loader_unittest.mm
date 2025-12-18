// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/url_session_url_loader.h"

#include <Foundation/Foundation.h>

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/test_future.h"
#include "base/test/test_suite.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/test/mock_url_loader_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using MockClient = testing::NiceMock<network::MockURLLoaderClient>;

namespace {

constexpr char kBody[] = "payload";
const std::string kTooBigPayload = std::string(1 << 21, 'a');

struct ResponseConfig {
  std::optional<std::string> body;
  bool os_error = false;
  bool hang = false;
  base::OnceClosure on_started;
  base::OnceClosure on_stopped;
};

std::string CreateSsoUrl(const size_t id) {
  return base::StrCat({"https://foobar.example.com/idp/idx/authenticators/"
                       "sso_extension/transactions/",
                       base::NumberToString(id), "/verify"});
}

// Since there is no simple way to pass data into NSURLProtocol, this registry
// provides a way to safely setup the expected behaviour for each request by
// using request urls as keys.
class ResponseRegistry {
 public:
  static ResponseRegistry* Get() {
    static base::NoDestructor<ResponseRegistry> instance;
    return instance.get();
  }

  std::string Register(ResponseConfig config) {
    base::AutoLock lock(lock_);
    const std::string url = CreateSsoUrl(counter_++);
    registry_[url] = std::move(config);
    return url;
  }

  ResponseConfig& ReadConfig(const std::string& url) {
    base::AutoLock lock(lock_);
    auto it = registry_.find(url);
    CHECK(it != registry_.end());
    return it->second;
  }

  void Clear() {
    base::AutoLock lock(lock_);
    registry_.clear();
  }

 private:
  friend class base::NoDestructor<ResponseRegistry>;
  ResponseRegistry() = default;

  base::Lock lock_;
  size_t counter_ = 0;
  std::map<std::string, ResponseConfig> registry_ GUARDED_BY(lock_);
};

}  // namespace

@interface MockProtocol : NSURLProtocol
@end

@implementation MockProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest*)request {
  return true;
}

+ (NSURLRequest*)canonicalRequestForRequest:(NSURLRequest*)request {
  return request;
}

- (void)startLoading {
  std::string gurl = net::GURLWithNSURL(self.request.URL).spec();
  ResponseConfig& config = ResponseRegistry::Get()->ReadConfig(gurl);
  if (config.on_started) {
    std::move(config.on_started).Run();
  }

  if (config.hang) {
    return;
  }

  if (config.os_error) {
    NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorNotConnectedToInternet
                                     userInfo:nil];
    [self.client URLProtocol:self didFailWithError:error];
    return;
  }

  NSData* data = nil;
  if (config.body.has_value()) {
    data = [NSData dataWithBytes:config.body.value().c_str()
                          length:config.body.value().size()];
  }

  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:self.request.URL
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:nil];

  [self.client URLProtocol:self
        didReceiveResponse:response
        cacheStoragePolicy:NSURLCacheStorageNotAllowed];
  [self.client URLProtocol:self didLoadData:data];
  [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading {
  std::string gurl = net::GURLWithNSURL(self.request.URL).spec();
  ResponseConfig& config = ResponseRegistry::Get()->ReadConfig(gurl);
  if (config.on_stopped) {
    std::move(config.on_stopped).Run();
  }
}

@end

namespace enterprise_auth {

class URLSessionURLLoaderTest : public testing::Test {
 protected:
  URLSessionURLLoaderTest()
      : client_receiver_(&mock_client,
                         client_remote_.InitWithNewPipeAndPassReceiver()) {
    client_receiver_.set_disconnect_handler(
        client_disconnect_future_.GetCallback());

    auto* instance = CreateURLSessionURLLoader();
    url_loader_ = instance->weak_ptr_factory_.GetWeakPtr();
    EXPECT_TRUE(url_loader_);
  }

  ~URLSessionURLLoaderTest() override { ResponseRegistry::Get()->Clear(); }

  MockClient& GetMockClient() { return mock_client; }

  // Should be called at most once per test instance.
  // The behaviour of the network is controlled by the url used, see the
  // anonymous namespace for details.
  void StartRequest(const std::string& url) {
    network::ResourceRequest request;
    request.url = GURL(url);
    request.method = "POST";
    url_loader_->Start(request, loader_remote_.BindNewPipeAndPassReceiver(),
                       std::move(client_remote_));
  }

  void WaitForLoaderToDisconnectAndDestroy() {
    EXPECT_TRUE(client_disconnect_future_.Wait());
    auto weak_ptr_copy = url_loader_;
    base::test::RunUntil(
        [weak_ptr_copy]() { return weak_ptr_copy.get() == nullptr; });
    client_receiver_.reset();
  }

  void DisconnectAndWaitForLoadersDestruction() {
    loader_remote_.reset();
    client_receiver_.reset();
    auto weak_ptr_copy = url_loader_;
    base::test::RunUntil(
        [weak_ptr_copy]() { return weak_ptr_copy.get() == nullptr; });
  }

  mojo::Remote<network::mojom::URLLoader> loader_remote_;

 private:
  static NSURLSession* CreateMockURLSession() {
    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.protocolClasses = @[ [MockProtocol class] ];
    return [NSURLSession sessionWithConfiguration:config];
  }

  URLSessionURLLoader* CreateURLSessionURLLoader() {
    URLSessionURLLoader* instance = new URLSessionURLLoader();
    instance->OverrideSessionForTesting(CreateMockURLSession());
    return instance;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::TestFuture<void> client_disconnect_future_;
  base::test::TestFuture<void> request_started_future_;

  base::WeakPtr<URLSessionURLLoader> url_loader_ = nullptr;

  MockClient mock_client;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_;
};

TEST_F(URLSessionURLLoaderTest, SuccessfulResponseWithBody) {
  ResponseConfig config;
  config.body = kBody;
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        std::string received_body;
        ASSERT_TRUE(body.is_valid());
        ASSERT_TRUE(
            mojo::BlockingCopyToString(std::move(body), &received_body));
        EXPECT_EQ(kBody, received_body);
        ASSERT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(url);
  WaitForLoaderToDisconnectAndDestroy();
}

TEST_F(URLSessionURLLoaderTest, SuccessfulResponseWithEmptyBody) {
  ResponseConfig config;
  config.body = "";
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        ASSERT_FALSE(body.is_valid());
        ASSERT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(url);
  WaitForLoaderToDisconnectAndDestroy();
}

TEST_F(URLSessionURLLoaderTest, SuccessfulResponseWithNoBody) {
  ResponseConfig config;
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        ASSERT_FALSE(body.is_valid());
        EXPECT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(url);
  WaitForLoaderToDisconnectAndDestroy();
}

TEST_F(URLSessionURLLoaderTest, RejectsTooBigBodies) {
  ResponseConfig config;
  config.body = kTooBigPayload;
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FILE_TOO_BIG)))
      .Times(1);

  StartRequest(url);
  WaitForLoaderToDisconnectAndDestroy();
}

TEST_F(URLSessionURLLoaderTest, DestroysItselfOnDisconnect) {
  ResponseConfig config;
  base::test::TestFuture<void> started_future;
  base::test::TestFuture<void> stopped_future;
  config.hang = true;
  config.on_started = started_future.GetSequenceBoundCallback();
  config.on_stopped = stopped_future.GetSequenceBoundCallback();
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(_)).Times(0);

  StartRequest(url);
  EXPECT_TRUE(started_future.Wait());

  DisconnectAndWaitForLoadersDestruction();
  EXPECT_TRUE(stopped_future.Wait());
}

TEST_F(URLSessionURLLoaderTest, HandlesErrorGently) {
  ResponseConfig config;
  config.os_error = true;
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  StartRequest(url);
  WaitForLoaderToDisconnectAndDestroy();
}

TEST_F(URLSessionURLLoaderTest, RequestCanceledOnDestruction) {
  ResponseConfig config;
  base::test::TestFuture<void> started_future;
  base::test::TestFuture<void> cancel_future;
  config.hang = true;
  config.on_started = started_future.GetSequenceBoundCallback();
  config.on_stopped = cancel_future.GetSequenceBoundCallback();
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(_)).Times(0);

  StartRequest(url);
  EXPECT_TRUE(started_future.Wait());
  DisconnectAndWaitForLoadersDestruction();
  EXPECT_TRUE(cancel_future.Wait());
}

TEST_F(URLSessionURLLoaderTest, WorksWithoutReceiver) {
  loader_remote_.reset();

  ResponseConfig config;
  config.body = kBody;
  const std::string url = ResponseRegistry::Get()->Register(std::move(config));

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        std::string received_body;
        ASSERT_TRUE(body.is_valid());
        ASSERT_TRUE(
            mojo::BlockingCopyToString(std::move(body), &received_body));
        EXPECT_EQ(kBody, received_body);
        ASSERT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(url);
  WaitForLoaderToDisconnectAndDestroy();
}

}  // namespace enterprise_auth
