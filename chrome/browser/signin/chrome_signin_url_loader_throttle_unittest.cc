// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_url_loader_throttle.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/header_modification_delegate.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Invoke;
using testing::Return;
using testing::_;

namespace signin {

namespace {

class MockDelegate : public HeaderModificationDelegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD1(ShouldInterceptNavigation,
               bool(content::NavigationUIData* navigation_ui_data));
  MOCK_METHOD2(ProcessRequest,
               void(ChromeRequestAdapter* request_adapter,
                    const GURL& redirect_url));
  MOCK_METHOD2(ProcessResponse,
               void(ResponseAdapter* response_adapter,
                    const GURL& redirect_url));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

content::WebContents::Getter NullWebContentsGetter() {
  return base::BindRepeating([]() -> content::WebContents* { return nullptr; });
}

}  // namespace

TEST(ChromeSigninURLLoaderThrottleTest, NoIntercept) {
  auto* delegate = new MockDelegate();

  EXPECT_CALL(*delegate, ShouldInterceptNavigation(_)).WillOnce(Return(false));
  EXPECT_FALSE(URLLoaderThrottle::MaybeCreate(base::WrapUnique(delegate),
                                              nullptr /* navigation_ui_data */,
                                              NullWebContentsGetter()));
}

TEST(ChromeSigninURLLoaderThrottleTest, Intercept) {
  auto* delegate = new MockDelegate();
  EXPECT_CALL(*delegate, ShouldInterceptNavigation(_)).WillOnce(Return(true));
  auto throttle = URLLoaderThrottle::MaybeCreate(
      base::WrapUnique(delegate), nullptr, NullWebContentsGetter());
  ASSERT_TRUE(throttle);

  // Phase 1: Start the request.

  const GURL kTestURL("https://google.com/index.html");
  const GURL kTestReferrer("https://chrome.com/referrer.html");
  base::MockCallback<base::OnceClosure> destruction_callback;
  EXPECT_CALL(*delegate, ProcessRequest(_, _))
      .WillOnce(
          Invoke([&](ChromeRequestAdapter* adapter, const GURL& redirect_url) {
            EXPECT_EQ(kTestURL, adapter->GetUrl());
            EXPECT_EQ(content::ResourceType::kMainFrame,
                      adapter->GetResourceType());
            EXPECT_EQ(GURL("https://chrome.com"), adapter->GetReferrerOrigin());

            EXPECT_TRUE(adapter->HasHeader("X-Request-1"));
            adapter->RemoveRequestHeaderByName("X-Request-1");
            EXPECT_FALSE(adapter->HasHeader("X-Request-1"));

            adapter->SetExtraHeaderByName("X-Request-2", "Bar");
            EXPECT_TRUE(adapter->HasHeader("X-Request-2"));

            EXPECT_EQ(GURL(), redirect_url);

            adapter->SetDestructionCallback(destruction_callback.Get());
          }));

  network::ResourceRequest request;
  request.url = kTestURL;
  request.referrer = kTestReferrer;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.headers.SetHeader("X-Request-1", "Foo");
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(request.headers.HasHeader("X-Request-1"));
  std::string value;
  EXPECT_TRUE(request.headers.GetHeader("X-Request-2", &value));
  EXPECT_EQ("Bar", value);

  EXPECT_FALSE(defer);

  testing::Mock::VerifyAndClearExpectations(delegate);

  // Phase 2: Redirect the request.

  const GURL kTestRedirectURL("https://youtube.com/index.html");
  const void* const kResponseUserDataKey = &kResponseUserDataKey;
  std::unique_ptr<base::SupportsUserData::Data> response_user_data =
      std::make_unique<base::SupportsUserData::Data>();
  base::SupportsUserData::Data* response_user_data_ptr =
      response_user_data.get();

  EXPECT_CALL(*delegate, ProcessResponse(_, _))
      .WillOnce(Invoke([&](ResponseAdapter* adapter, const GURL& redirect_url) {
        EXPECT_EQ(GURL("https://google.com"), adapter->GetOrigin());
        EXPECT_TRUE(adapter->IsMainFrame());

        adapter->SetUserData(kResponseUserDataKey,
                             std::move(response_user_data));
        EXPECT_EQ(response_user_data_ptr,
                  adapter->GetUserData(kResponseUserDataKey));

        const net::HttpResponseHeaders* headers = adapter->GetHeaders();
        EXPECT_TRUE(headers->HasHeader("X-Response-1"));
        EXPECT_TRUE(headers->HasHeader("X-Response-2"));
        adapter->RemoveHeader("X-Response-2");

        EXPECT_EQ(kTestRedirectURL, redirect_url);
      }));

  base::MockCallback<base::OnceClosure> ignored_destruction_callback;
  EXPECT_CALL(*delegate, ProcessRequest(_, _))
      .WillOnce(
          Invoke([&](ChromeRequestAdapter* adapter, const GURL& redirect_url) {
            EXPECT_EQ(content::ResourceType::kMainFrame,
                      adapter->GetResourceType());

            // Changes to the URL and referrer take effect after the redirect
            // is followed.
            EXPECT_EQ(kTestURL, adapter->GetUrl());
            EXPECT_EQ(GURL("https://chrome.com"), adapter->GetReferrerOrigin());

            // X-Request-1 and X-Request-2 were modified in the previous call to
            // ProcessRequest(). These changes should still be present.
            EXPECT_FALSE(adapter->HasHeader("X-Request-1"));
            EXPECT_TRUE(adapter->HasHeader("X-Request-2"));

            adapter->RemoveRequestHeaderByName("X-Request-2");
            EXPECT_FALSE(adapter->HasHeader("X-Request-2"));

            adapter->SetExtraHeaderByName("X-Request-3", "Baz");
            EXPECT_TRUE(adapter->HasHeader("X-Request-3"));

            EXPECT_EQ(kTestRedirectURL, redirect_url);

            adapter->SetDestructionCallback(ignored_destruction_callback.Get());
          }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = kTestRedirectURL;
  // An HTTPS to HTTPS redirect such as this wouldn't normally change the
  // referrer but we do for testing purposes.
  redirect_info.new_referrer = kTestURL.spec();

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->AddHeader("X-Response-1: Foo");
  response_head->headers->AddHeader("X-Response-2: Bar");

  std::vector<std::string> request_headers_to_remove;
  net::HttpRequestHeaders modified_request_headers;
  throttle->WillRedirectRequest(&redirect_info, *response_head, &defer,
                                &request_headers_to_remove,
                                &modified_request_headers);

  EXPECT_FALSE(defer);

  EXPECT_TRUE(response_head->headers->HasHeader("X-Response-1"));
  EXPECT_FALSE(response_head->headers->HasHeader("X-Response-2"));

  EXPECT_THAT(request_headers_to_remove, ElementsAre("X-Request-2"));
  EXPECT_TRUE(modified_request_headers.GetHeader("X-Request-3", &value));
  EXPECT_EQ("Baz", value);

  testing::Mock::VerifyAndClearExpectations(delegate);

  // Phase 3: Complete the request.

  EXPECT_CALL(*delegate, ProcessResponse(_, _))
      .WillOnce(Invoke([&](ResponseAdapter* adapter, const GURL& redirect_url) {
        EXPECT_EQ(GURL("https://youtube.com"), adapter->GetOrigin());
        EXPECT_TRUE(adapter->IsMainFrame());

        EXPECT_EQ(response_user_data_ptr,
                  adapter->GetUserData(kResponseUserDataKey));

        const net::HttpResponseHeaders* headers = adapter->GetHeaders();
        // This is a new response and so previous headers should not carry over.
        EXPECT_FALSE(headers->HasHeader("X-Response-1"));
        EXPECT_FALSE(headers->HasHeader("X-Response-2"));

        EXPECT_TRUE(headers->HasHeader("X-Response-3"));
        EXPECT_TRUE(headers->HasHeader("X-Response-4"));
        adapter->RemoveHeader("X-Response-3");

        EXPECT_EQ(GURL(), redirect_url);
      }));

  response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->AddHeader("X-Response-3: Foo");
  response_head->headers->AddHeader("X-Response-4: Bar");

  throttle->WillProcessResponse(kTestRedirectURL, response_head.get(), &defer);

  EXPECT_FALSE(response_head->headers->HasHeader("X-Response-3"));
  EXPECT_TRUE(response_head->headers->HasHeader("X-Response-4"));

  EXPECT_FALSE(defer);

  EXPECT_CALL(destruction_callback, Run()).Times(1);
  EXPECT_CALL(ignored_destruction_callback, Run()).Times(0);
  throttle.reset();
}

TEST(ChromeSigninURLLoaderThrottleTest, InterceptSubFrame) {
  auto* delegate = new MockDelegate();
  EXPECT_CALL(*delegate, ShouldInterceptNavigation(_)).WillOnce(Return(true));
  auto throttle = URLLoaderThrottle::MaybeCreate(
      base::WrapUnique(delegate), nullptr, NullWebContentsGetter());
  ASSERT_TRUE(throttle);

  EXPECT_CALL(*delegate, ProcessRequest(_, _))
      .Times(2)
      .WillRepeatedly([](ChromeRequestAdapter* adapter,
                         const GURL& redirect_url) {
        EXPECT_EQ(content::ResourceType::kSubFrame, adapter->GetResourceType());
      });

  network::ResourceRequest request;
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::ResourceType::kSubFrame);

  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_CALL(*delegate, ProcessResponse(_, _))
      .Times(2)
      .WillRepeatedly(([](ResponseAdapter* adapter, const GURL& redirect_url) {
        EXPECT_FALSE(adapter->IsMainFrame());
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://youtube.com");
  auto response_head = network::mojom::URLResponseHead::New();

  std::vector<std::string> request_headers_to_remove;
  net::HttpRequestHeaders modified_request_headers;
  throttle->WillRedirectRequest(&redirect_info, *response_head, &defer,
                                &request_headers_to_remove,
                                &modified_request_headers);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(request_headers_to_remove.empty());
  EXPECT_TRUE(modified_request_headers.IsEmpty());

  throttle->WillProcessResponse(GURL("https://youtube.com"),
                                response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

}  // namespace signin
