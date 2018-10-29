// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/digital_asset_links/digital_asset_links_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace digital_asset_links {
namespace {

class DigitalAssetLinksHandlerTest : public ::testing::Test {
 public:
  DigitalAssetLinksHandlerTest()
      : num_invocations_(0),
        result_(RelationshipCheckResult::SUCCESS),
        io_thread_(content::BrowserThread::IO,
                   base::ThreadTaskRunnerHandle::Get()) {}

  void OnRelationshipCheckComplete(RelationshipCheckResult result) {
    ++num_invocations_;
    result_ = result;
  }

 protected:
  void SetUp() override { num_invocations_ = 0; }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void AddResponse(net::Error error, int response_code, bool linked) {
    std::string response_string;
    if (error == net::OK && response_code == net::HTTP_OK && linked) {
      response_string =
          R"({
            "linked":  true ,
            "maxAge": "40.188652381s"
          })";
    } else if (error == net::OK && response_code == net::HTTP_OK) {
      response_string =
          R"({
            "linked":  false ,
            "maxAge": "40.188652381s"
          })";
    } else if (error == net::OK && response_code == net::HTTP_BAD_REQUEST) {
      response_string =
          R"({
            "code":  400 ,
            "message": "Invalid statement query received."
            "status": "INVALID_ARGUMENT"
          })";
    }
    auto& url = test_url_loader_factory_.pending_requests()->at(0).request.url;
    if (response_string.empty()) {
      network::ResourceResponseHead response_head;
      std::string status_line =
          "HTTP/1.1 " + base::NumberToString(response_code) + " " +
          net::GetHttpReasonPhrase(
              static_cast<net::HttpStatusCode>(response_code));
      response_head.headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(status_line);
      test_url_loader_factory_.AddResponse(
          url, response_head, response_string,
          network::URLLoaderCompletionStatus(error));
    } else {
      test_url_loader_factory_.AddResponse(
          url.spec(), response_string,
          static_cast<net::HttpStatusCode>(response_code));
    }
    base::RunLoop().RunUntilIdle();
  }

  int num_invocations_;
  RelationshipCheckResult result_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;
  content::TestBrowserThread io_thread_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(DigitalAssetLinksHandlerTest);
};
}  // namespace

TEST_F(DigitalAssetLinksHandlerTest, PositiveResponse) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::Bind(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                 base::Unretained(this)),
      "", "", "", "");
  AddResponse(net::OK, net::HTTP_OK, true);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::SUCCESS);
}

TEST_F(DigitalAssetLinksHandlerTest, NegativeResponse) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::Bind(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                 base::Unretained(this)),
      "", "", "", "");
  AddResponse(net::OK, net::HTTP_OK, false);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, BadRequest) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::Bind(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                 base::Unretained(this)),
      "", "", "", "");
  AddResponse(net::OK, net::HTTP_BAD_REQUEST, true);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, NetworkError) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::Bind(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                 base::Unretained(this)),
      "", "", "", "");
  AddResponse(net::ERR_ABORTED, net::HTTP_OK, true);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, NetworkDisconnected) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::Bind(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                 base::Unretained(this)),
      "", "", "", "");
  AddResponse(net::ERR_INTERNET_DISCONNECTED, net::HTTP_OK, true);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::NO_CONNECTION);
}
}  // namespace digital_asset_links
