// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/send_message_express.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/fake_token_fetcher.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOAuthToken[] = "oauth_token";
const char kTestAccount[] = "test@test.test";

chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
CreateRequest() {
  return chrome_browser_nearby_sharing_instantmessaging::
      SendMessageExpressRequest();
}

}  // namespace

class SendMessageExpressTest : public testing::Test {
 public:
  SendMessageExpressTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        send_message_express_(identity_test_environment_.identity_manager(),
                              test_shared_loader_factory_) {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kTestAccount, signin::ConsentLevel::kSignin);
  }
  ~SendMessageExpressTest() override = default;

  void SetOAuthTokenSuccessful(bool success) {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            success ? kOAuthToken : "", base::Time::Now());
  }

  SendMessageExpress& GetMessenger() { return send_message_express_; }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return test_url_loader_factory_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  SendMessageExpress send_message_express_;
};

TEST_F(SendMessageExpressTest, OAuthTokenFailed) {
  base::RunLoop run_loop;
  GetMessenger().SendMessage(CreateRequest(),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               run_loop.Quit();
                             }));
  SetOAuthTokenSuccessful(false);
  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());
  run_loop.Run();
}

TEST_F(SendMessageExpressTest, HttpResponseError) {
  base::RunLoop run_loop;
  GetMessenger().SendMessage(CreateRequest(),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               run_loop.Quit();
                             }));
  SetOAuthTokenSuccessful(true);
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI,
                                        "response", net::HTTP_FORBIDDEN);
  run_loop.Run();
}

TEST_F(SendMessageExpressTest, EmptyResponse) {
  base::RunLoop run_loop;
  GetMessenger().SendMessage(CreateRequest(),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               run_loop.Quit();
                             }));
  SetOAuthTokenSuccessful(true);
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI, "",
                                        net::HTTP_OK);
  run_loop.Run();
}

TEST_F(SendMessageExpressTest, SuccessfulResponse) {
  base::RunLoop run_loop;

  GetMessenger().SendMessage(CreateRequest(),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
  SetOAuthTokenSuccessful(true);
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI,
                                        "response", net::HTTP_OK);
  run_loop.Run();
}
