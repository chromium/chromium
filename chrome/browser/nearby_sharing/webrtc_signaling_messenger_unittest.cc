// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSelfId[] = "self_id";
const char kOAuthToken[] = "oauth_token";
const char kTestAccount[] = "test@test.test";
const char kCountryCode[] = "ZZ";

class FakeIncomingMessagesListener
    : public ::sharing::mojom::IncomingMessagesListener {
 public:
  ~FakeIncomingMessagesListener() override = default;

  void OnMessage(const std::string& message) override {
    messages_received_.push_back(message);
  }

  const std::vector<std::string>& messages_received() {
    return messages_received_;
  }

 private:
  std::vector<std::string> messages_received_;
};

class WebRtcSignalingMessengerTest : public testing::Test {
 public:
  WebRtcSignalingMessengerTest()
      : webrtc_signaling_messenger_(
            identity_test_environment_.identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~WebRtcSignalingMessengerTest() override = default;

  void SetUp() override {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kTestAccount, signin::ConsentLevel::kSignin);
  }

  void SetOAuthTokenSuccessful(bool success) {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            success ? kOAuthToken : "", base::Time::Now());
  }

  WebRtcSignalingMessenger& GetMessenger() {
    return webrtc_signaling_messenger_;
  }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return test_url_loader_factory_;
  }

  ::sharing::mojom::LocationHintPtr CountryCodeLocationHint(
      std::string country_code) {
    ::sharing::mojom::LocationHintPtr location_hint_ptr =
        ::sharing::mojom::LocationHint::New();
    location_hint_ptr->location = country_code;
    location_hint_ptr->format =
        ::sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
    return location_hint_ptr;
  }

  // Required to ensure that the listener has received all messages before we
  // can continue with our tests.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  WebRtcSignalingMessenger webrtc_signaling_messenger_;
};

TEST_F(WebRtcSignalingMessengerTest, UnsuccessfulSendMessage_EmptyToken) {
  base::RunLoop loop;
  GetMessenger().SendMessage(kSelfId, "peer_id",
                             CountryCodeLocationHint(kCountryCode), "message",
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               loop.Quit();
                             }));
  SetOAuthTokenSuccessful(/*success=*/false);
  loop.Run();
}

TEST_F(WebRtcSignalingMessengerTest, UnsuccessfulSendMessage_HttpError) {
  base::RunLoop loop;
  GetMessenger().SendMessage(kSelfId, "peer_id",
                             CountryCodeLocationHint(kCountryCode), "message",
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               loop.Quit();
                             }));
  SetOAuthTokenSuccessful(/*success=*/true);

  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI,
                                        "response", net::HTTP_FORBIDDEN);

  loop.Run();
}

TEST_F(WebRtcSignalingMessengerTest, SuccessfulSendMessage) {
  base::RunLoop loop;
  GetMessenger().SendMessage(kSelfId, "peer_id",
                             CountryCodeLocationHint(kCountryCode), "message",
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               loop.Quit();
                             }));
  SetOAuthTokenSuccessful(/*success=*/true);

  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI,
                                        "response", net::HTTP_OK);

  loop.Run();
}

}  // namespace
