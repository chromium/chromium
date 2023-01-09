// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/remote_trust_token_query_answerer.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

constexpr char kMainFrameOrigin[] = "https://www.example.com/";

// A fake TrustTokenQueryAnswerer that replies with canned answers and has the
// instrumentation to support a mojo disconnect.
class FakeTrustTokenQueryAnswerer
    : public network::mojom::TrustTokenQueryAnswerer {
 public:
  // Implementation of network::mojom::TrustTokenQueryAnswerer
  void HasTrustTokens(const url::Origin& issuer,
                      HasTrustTokensCallback callback) override {
    if (!callbacks_enabled_) {
      // Grab the callback and hold it. The callback will never be invoked and
      // is just held until the pipe is closed since Mojo DCHECKs if they're
      // deleted earlier.
      has_tokens_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(network::mojom::HasTrustTokensResult::New(
        network::mojom::TrustTokenOperationStatus::kOk,
        /*has_trust_tokens=*/false));
  }

  void HasRedemptionRecord(const url::Origin& issuer,
                           HasRedemptionRecordCallback callback) override {
    if (!callbacks_enabled_) {
      // Grab the callback and hold it. The callback will never be invoked and
      // is just held until the pipe is closed since Mojo DCHECKs if they're
      // deleted earlier.
      has_redemption_record_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(network::mojom::HasRedemptionRecordResult::New(
        network::mojom::TrustTokenOperationStatus::kOk,
        /*has_redemption_record=*/false));
  }

  void DisableTrustTokenQueryCallbacks() { callbacks_enabled_ = false; }

 protected:
  bool callbacks_enabled_ = true;
  HasTrustTokensCallback has_tokens_callback_;
  HasRedemptionRecordCallback has_redemption_record_callback_;
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext()
      : answerer_(std::make_unique<FakeTrustTokenQueryAnswerer>()),
        self_(this) {}

  // Creates a new mojo receiver to replace the existing one. Should only be
  // called when the current receiver is disconnected.
  void GetTrustTokenQueryAnswerer(
      mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver,
      const url::Origin& top_frame_origin) override {
    answerer_receiver_ = std::make_unique<
        mojo::Receiver<network::mojom::TrustTokenQueryAnswerer>>(
        answerer_.get(), std::move(receiver));
  }

  mojo::PendingRemote<network::mojom::NetworkContext> CreatePendingRemote() {
    return self_.BindNewPipeAndPassRemote();
  }

  void DisconnectTrustTokenQueryAnswerer() {
    answerer_receiver_->reset();
    ASSERT_FALSE(answerer_receiver_->is_bound());
    answerer_ = std::make_unique<FakeTrustTokenQueryAnswerer>();
  }

  void DisableTrustTokenQueryCallbacks() {
    answerer_->DisableTrustTokenQueryCallbacks();
  }

 private:
  std::unique_ptr<FakeTrustTokenQueryAnswerer> answerer_;
  mojo::Receiver<network::mojom::NetworkContext> self_;
  std::unique_ptr<mojo::Receiver<network::mojom::TrustTokenQueryAnswerer>>
      answerer_receiver_;
};

class RemoteTrustTokenQueryAnswererTest : public testing::Test {
 public:
  RemoteTrustTokenQueryAnswererTest()
      : issuer_(url::Origin::Create(GURL(kMainFrameOrigin))) {
    DCHECK(network::IsOriginPotentiallyTrustworthy(issuer_));
    DCHECK(issuer_.scheme() == url::kHttpsScheme);
    profile_.GetDefaultStoragePartition()->SetNetworkContextForTesting(
        context_.CreatePendingRemote());
  }

  std::unique_ptr<RemoteTrustTokenQueryAnswerer> CreateAnswerer() {
    return std::make_unique<RemoteTrustTokenQueryAnswerer>(issuer_, &profile_);
  }

  network::mojom::HasTrustTokensResultPtr CallHasTrustTokensAndWait(
      RemoteTrustTokenQueryAnswerer* answerer) {
    network::mojom::HasTrustTokensResultPtr has_tokens;
    base::RunLoop run_loop;
    answerer->HasTrustTokens(
        issuer_, base::BindLambdaForTesting(
                     [&run_loop, &has_tokens](
                         network::mojom::HasTrustTokensResultPtr result) {
                       has_tokens = std::move(result);
                       run_loop.Quit();
                     }));
    run_loop.Run();
    return has_tokens;
  }

  network::mojom::HasRedemptionRecordResultPtr CallHasRedemptionRecordAndWait(
      RemoteTrustTokenQueryAnswerer* answerer) {
    network::mojom::HasRedemptionRecordResultPtr has_redemption_record;
    base::RunLoop run_loop;
    answerer->HasRedemptionRecord(
        issuer_, base::BindLambdaForTesting(
                     [&run_loop, &has_redemption_record](
                         network::mojom::HasRedemptionRecordResultPtr result) {
                       has_redemption_record = std::move(result);
                       run_loop.Quit();
                     }));
    run_loop.Run();
    return has_redemption_record;
  }

  void DisconnectTrustTokenQueryAnswerer() {
    context_.DisconnectTrustTokenQueryAnswerer();
  }

  void DisableTrustTokenQueryCallbacks() {
    context_.DisableTrustTokenQueryCallbacks();
  }

  const url::Origin& issuer() const { return issuer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  url::Origin issuer_;
  FakeNetworkContext context_;
  TestingProfile profile_;
};

TEST_F(RemoteTrustTokenQueryAnswererTest, SurvivesDisconnectBetweenCalls) {
  TestingProfile profile;
  auto answerer = CreateAnswerer();
  auto has_tokens_result = CallHasTrustTokensAndWait(answerer.get());
  auto has_redemption_record_result =
      CallHasRedemptionRecordAndWait(answerer.get());
  ASSERT_TRUE(has_tokens_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_tokens_result->status);
  EXPECT_FALSE(has_tokens_result->has_trust_tokens);
  ASSERT_TRUE(has_redemption_record_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_redemption_record_result->status);
  EXPECT_FALSE(has_redemption_record_result->has_redemption_record);

  // Disconnect
  DisconnectTrustTokenQueryAnswerer();

  // |answerer| will receive an error notification, but it's not
  // guaranteed to have arrived at this point. Flush the remote to make sure
  // the notification has been received.
  answerer->FlushForTesting();

  has_tokens_result = CallHasTrustTokensAndWait(answerer.get());
  has_redemption_record_result = CallHasRedemptionRecordAndWait(answerer.get());
  ASSERT_TRUE(has_tokens_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_tokens_result->status);
  EXPECT_FALSE(has_tokens_result->has_trust_tokens);
  ASSERT_TRUE(has_redemption_record_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_redemption_record_result->status);
  EXPECT_FALSE(has_redemption_record_result->has_redemption_record);
}

TEST_F(RemoteTrustTokenQueryAnswererTest, SurvivesDisconnectInHasTrustTokens) {
  TestingProfile profile;
  auto answerer = CreateAnswerer();

  auto has_tokens_result = CallHasTrustTokensAndWait(answerer.get());
  ASSERT_TRUE(has_tokens_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_tokens_result->status);
  EXPECT_FALSE(has_tokens_result->has_trust_tokens);

  {
    base::RunLoop run_loop;
    DisableTrustTokenQueryCallbacks();
    answerer->HasTrustTokens(
        issuer(), base::BindLambdaForTesting(
                      [&run_loop, &has_tokens_result](
                          network::mojom::HasTrustTokensResultPtr result) {
                        has_tokens_result = std::move(result);
                        run_loop.Quit();
                      }));
    DisconnectTrustTokenQueryAnswerer();
    run_loop.Run();
    EXPECT_FALSE(has_tokens_result);
  }

  has_tokens_result = CallHasTrustTokensAndWait(answerer.get());
  ASSERT_TRUE(has_tokens_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_tokens_result->status);
  EXPECT_FALSE(has_tokens_result->has_trust_tokens);
}

TEST_F(RemoteTrustTokenQueryAnswererTest,
       SurvivesDisconnectInHasRedemptionRecord) {
  TestingProfile profile;
  auto answerer = CreateAnswerer();

  auto has_redemption_record_result =
      CallHasRedemptionRecordAndWait(answerer.get());
  ASSERT_TRUE(has_redemption_record_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_redemption_record_result->status);
  EXPECT_FALSE(has_redemption_record_result->has_redemption_record);

  {
    base::RunLoop run_loop;
    DisableTrustTokenQueryCallbacks();
    answerer->HasRedemptionRecord(
        issuer(), base::BindLambdaForTesting(
                      [&run_loop, &has_redemption_record_result](
                          network::mojom::HasRedemptionRecordResultPtr result) {
                        has_redemption_record_result = std::move(result);
                        run_loop.Quit();
                      }));
    DisconnectTrustTokenQueryAnswerer();
    run_loop.Run();
    EXPECT_FALSE(has_redemption_record_result);
  }

  has_redemption_record_result = CallHasRedemptionRecordAndWait(answerer.get());
  ASSERT_TRUE(has_redemption_record_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_redemption_record_result->status);
  EXPECT_FALSE(has_redemption_record_result->has_redemption_record);
}

TEST_F(RemoteTrustTokenQueryAnswererTest,
       HasTrustTokensHandlesSynchronousCall) {
  TestingProfile profile;
  auto answerer = CreateAnswerer();

  auto has_tokens_result = CallHasTrustTokensAndWait(answerer.get());
  ASSERT_TRUE(has_tokens_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_tokens_result->status);
  EXPECT_FALSE(has_tokens_result->has_trust_tokens);

  {
    base::RunLoop run_loop;
    answerer->HasTrustTokens(
        issuer(),
        base::BindLambdaForTesting(
            [&](network::mojom::HasTrustTokensResultPtr result) {
              ASSERT_TRUE(result);
              EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
                        result->status);
              EXPECT_FALSE(result->has_trust_tokens);

              answerer->HasTrustTokens(
                  issuer(),
                  base::BindLambdaForTesting(
                      [&](network::mojom::HasTrustTokensResultPtr result) {
                        ASSERT_TRUE(result);
                        EXPECT_EQ(
                            network::mojom::TrustTokenOperationStatus::kOk,
                            result->status);
                        EXPECT_FALSE(result->has_trust_tokens);
                        run_loop.Quit();
                      }));
            }));
    run_loop.Run();
  }
}

TEST_F(RemoteTrustTokenQueryAnswererTest,
       HasRedemptionRecordHandlesSynchronousCall) {
  TestingProfile profile;
  auto answerer = CreateAnswerer();

  auto has_tokens_result = CallHasRedemptionRecordAndWait(answerer.get());
  ASSERT_TRUE(has_tokens_result);
  EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
            has_tokens_result->status);
  EXPECT_FALSE(has_tokens_result->has_redemption_record);

  {
    base::RunLoop run_loop;
    answerer->HasRedemptionRecord(
        issuer(),
        base::BindLambdaForTesting(
            [&](network::mojom::HasRedemptionRecordResultPtr result) {
              ASSERT_TRUE(result);
              EXPECT_EQ(network::mojom::TrustTokenOperationStatus::kOk,
                        result->status);
              EXPECT_FALSE(result->has_redemption_record);

              answerer->HasRedemptionRecord(
                  issuer(),
                  base::BindLambdaForTesting(
                      [&](network::mojom::HasRedemptionRecordResultPtr result) {
                        ASSERT_TRUE(result);
                        EXPECT_EQ(
                            network::mojom::TrustTokenOperationStatus::kOk,
                            result->status);
                        EXPECT_FALSE(result->has_redemption_record);
                        run_loop.Quit();
                      }));
            }));
    run_loop.Run();
  }
}

}  // namespace
