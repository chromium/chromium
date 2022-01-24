// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_xhr_sender.h"

#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestUserEmail[] = "testuser1@gmail.com";
const base::TimeDelta kExpiryTimeFromNow = base::Minutes(10);
constexpr char kValidUrl[] = "https://www.googleapis.com/drive/v3/files/fileID";
constexpr char kValidUrl2[] =
    "https://translation.googleapis.com/language/translate/v2";

}  // namespace

namespace ash {

class ProjectorXhrSenderTest : public testing::Test {
 public:
  ProjectorXhrSenderTest() = default;
  ProjectorXhrSenderTest(const ProjectorXhrSenderTest&) = delete;
  ProjectorXhrSenderTest& operator=(const ProjectorXhrSenderTest&) = delete;
  ~ProjectorXhrSenderTest() override = default;

  // testing::Test:
  void SetUp() override {
    sender_ = std::make_unique<ProjectorXhrSender>(
        mock_app_client_.GetUrlLoaderFactory());
  }

  ProjectorXhrSender* sender() { return sender_.get(); }
  MockAppClient& mock_app_client() { return mock_app_client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ProjectorXhrSender> sender_;
  MockAppClient mock_app_client_;
};

TEST_F(ProjectorXhrSenderTest, Success) {
  base::RunLoop run_loop;

  const std::string& test_response_body = "{}";
  sender()->Send(
      GURL(kValidUrl), "GET", /*request_body=*/"", /*useCredential=*/false,
      base::BindOnce(
          [](const std::string& expected_response_body,
             base::RepeatingClosure quit_closure, bool success,
             const std::string& response_body, const std::string& error) {
            EXPECT_TRUE(success);
            EXPECT_EQ(expected_response_body, response_body);
            EXPECT_EQ("", error);
            quit_closure.Run();
          },
          test_response_body, run_loop.QuitClosure()));

  mock_app_client().test_url_loader_factory().AddResponse(kValidUrl,
                                                          test_response_body);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  run_loop.Run();
}

TEST_F(ProjectorXhrSenderTest, TwoRequests) {
  base::RunLoop run_loop;
  const std::string& test_response_body = "{}";
  sender()->Send(
      GURL(kValidUrl), "GET", /*request_body=*/"", /*useCredential=*/false,
      base::BindOnce(
          [](const std::string& expected_response_body,
             base::RepeatingClosure quit_closure, bool success,
             const std::string& response_body, const std::string& error) {
            EXPECT_TRUE(success);
            EXPECT_EQ(expected_response_body, response_body);
            EXPECT_EQ("", error);
            quit_closure.Run();
          },
          test_response_body, run_loop.QuitClosure()));

  base::RunLoop run_loop2;
  const std::string& test_response_body2 = "{data: {}}";
  sender()->Send(
      GURL(kValidUrl2), "GET", /*request_body=*/"", /*useCredential=*/false,
      base::BindOnce(
          [](const std::string& expected_response_body,
             base::RepeatingClosure quit_closure, bool success,
             const std::string& response_body, const std::string& error) {
            EXPECT_TRUE(success);
            EXPECT_EQ(expected_response_body, response_body);
            EXPECT_EQ("", error);
            quit_closure.Run();
          },
          test_response_body2, run_loop2.QuitClosure()));

  mock_app_client().test_url_loader_factory().AddResponse(kValidUrl,
                                                          test_response_body);

  mock_app_client().test_url_loader_factory().AddResponse(kValidUrl2,
                                                          test_response_body2);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  run_loop.Run();
  run_loop2.Run();
}

TEST_F(ProjectorXhrSenderTest, UseCredentials) {
  base::RunLoop run_loop;

  const std::string& test_response_body = "{}";
  sender()->Send(
      GURL(kValidUrl), "GET", /*request_body=*/"", /*useCredential=*/true,
      base::BindOnce(
          [](const std::string& expected_response_body,
             base::RepeatingClosure quit_closure, bool success,
             const std::string& response_body, const std::string& error) {
            EXPECT_TRUE(success);
            EXPECT_EQ(expected_response_body, response_body);
            EXPECT_EQ("", error);
            quit_closure.Run();
          },
          test_response_body, run_loop.QuitClosure()));

  // Verify that http request is sent without granting OAuth token.
  mock_app_client().test_url_loader_factory().AddResponse(kValidUrl,
                                                          test_response_body);

  run_loop.Run();
}

TEST_F(ProjectorXhrSenderTest, NetworkError) {
  base::RunLoop run_loop;

  sender()->Send(
      GURL(kValidUrl), /*method=*/"GET", /*request_body=*/"",
      /*use_credentials=*/false,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool success,
             const std::string& response_body, const std::string& error) {
            EXPECT_FALSE(success);
            EXPECT_EQ("", response_body);
            EXPECT_EQ("XHR_FETCH_FAILURE", error);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));

  mock_app_client().test_url_loader_factory().AddResponse(
      GURL(kValidUrl), network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  run_loop.Run();
}

TEST_F(ProjectorXhrSenderTest, UnsupportedUrl) {
  base::RunLoop run_loop;

  sender()->Send(
      GURL("https://example.com"), /*method=*/"GET", /*request_body=*/"",
      /*use_credentials=*/false,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool success,
             const std::string& response_body, const std::string& error) {
            EXPECT_FALSE(success);
            EXPECT_EQ("", response_body);
            EXPECT_EQ("UNSUPPORTED_URL", error);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace ash
