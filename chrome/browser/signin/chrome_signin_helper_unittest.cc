// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(ENABLE_MIRROR) || BUILDFLAG(IS_CHROMEOS_ASH)
const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";
const char kMirrorAction[] = "action=ADDSESSION";
#endif

// URLRequestInterceptor adding a account consistency response header to Gaia
// responses.
class TestRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit TestRequestInterceptor(const std::string& header_name,
                                  const std::string& header_value)
      : header_name_(header_name), header_value_(header_value) {}
  ~TestRequestInterceptor() override = default;

 private:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    std::string response_headers =
        base::StringPrintf("HTTP/1.1 200 OK\n\n%s: %s\n", header_name_.c_str(),
                           header_value_.c_str());
    return std::make_unique<net::URLRequestTestJob>(request, response_headers,
                                                    "", true);
  }

  const std::string header_name_;
  const std::string header_value_;
};

class TestResponseAdapter : public signin::ResponseAdapter,
                            public base::SupportsUserData {
 public:
  TestResponseAdapter(const std::string& header_name,
                      const std::string& header_value,
                      bool is_main_frame)
      : is_main_frame_(is_main_frame),
        headers_(new net::HttpResponseHeaders(std::string())) {
    headers_->SetHeader(header_name, header_value);
  }

  ~TestResponseAdapter() override {}

  content::WebContents::Getter GetWebContentsGetter() const override {
    return base::BindRepeating(
        []() -> content::WebContents* { return nullptr; });
  }
  bool IsMainFrame() const override { return is_main_frame_; }
  GURL GetOrigin() const override {
    return GURL("https://accounts.google.com");
  }
  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_.get();
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

  base::SupportsUserData::Data* GetUserData(const void* key) const override {
    return base::SupportsUserData::GetUserData(key);
  }

  void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) override {
    return base::SupportsUserData::SetUserData(key, std::move(data));
  }

 private:
  bool is_main_frame_;
  scoped_refptr<net::HttpResponseHeaders> headers_;

  DISALLOW_COPY_AND_ASSIGN(TestResponseAdapter);
};

}  // namespace

class ChromeSigninHelperTest : public testing::Test {
 protected:
  ChromeSigninHelperTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  ~ChromeSigninHelperTest() override {}

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<net::TestDelegate> test_request_delegate_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Tests that Dice response headers are removed after being processed.
TEST_F(ChromeSigninHelperTest, RemoveDiceSigninHeader) {
  // Process the header.
  TestResponseAdapter adapter(signin::kDiceResponseHeader, "Foo",
                              /*is_main_frame=*/false);
  signin::ProcessAccountConsistencyResponseHeaders(&adapter, GURL(),
                                                   false /* is_incognito */);

  // Check that the header has been removed.
  EXPECT_FALSE(adapter.GetHeaders()->HasHeader(signin::kDiceResponseHeader));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_MIRROR) || BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that user data is set on Mirror requests.
TEST_F(ChromeSigninHelperTest, MirrorMainFrame) {
  // Process the header.
  TestResponseAdapter response_adapter(kChromeManageAccountsHeader,
                                       kMirrorAction,
                                       /*is_main_frame=*/true);
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);
  // Check that the header has not been removed.
  EXPECT_TRUE(
      response_adapter.GetHeaders()->HasHeader(kChromeManageAccountsHeader));
  // Request was flagged with the user data.
  EXPECT_TRUE(response_adapter.GetUserData(
      signin::kManageAccountsHeaderReceivedUserDataKey));
}

// Tests that user data is not set on Mirror requests for sub frames.
TEST_F(ChromeSigninHelperTest, MirrorSubFrame) {
  // Process the header.
  TestResponseAdapter response_adapter(kChromeManageAccountsHeader,
                                       kMirrorAction,
                                       /*is_main_frame=*/false);
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);
  // Request was not flagged with the user data.
  EXPECT_FALSE(response_adapter.GetUserData(
      signin::kManageAccountsHeaderReceivedUserDataKey));
}
#endif  // BUILDFLAG(ENABLE_MIRROR) || BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ChromeSigninHelperTest,
       ParseGaiaIdFromRemoveLocalAccountResponseHeader) {
  EXPECT_EQ("123456",
            signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
                TestResponseAdapter("Google-Accounts-RemoveLocalAccount",
                                    "obfuscatedid=\"123456\"",
                                    /*is_main_frame=*/false)
                    .GetHeaders()));
  EXPECT_EQ("123456",
            signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
                TestResponseAdapter("Google-Accounts-RemoveLocalAccount",
                                    "obfuscatedid=\"123456\",foo=\"bar\"",
                                    /*is_main_frame=*/false)
                    .GetHeaders()));
  EXPECT_EQ(
      "",
      signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
          TestResponseAdapter("Google-Accounts-RemoveLocalAccount", "malformed",
                              /*is_main_frame=*/false)
              .GetHeaders()));
}
