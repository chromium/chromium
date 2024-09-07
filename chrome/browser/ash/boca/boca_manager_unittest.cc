// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "components/account_id/account_id.h"
#include "google_apis/common/request_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace ash {
namespace {

constexpr char kTestEmail[] = "testemail";

class MockSessionClientImpl : public boca::SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
  MOCK_METHOD(void,
              GetSession,
              (std::unique_ptr<boca::GetSessionRequest>),
              (override));
};

class BocaManagerProducerTest : public testing::Test {
 protected:
  BocaManagerProducerTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({ash::features::kBoca},
                                          /*disabled_features=*/{});

    auto session_client_impl =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);
    auto* const session_client_impl_ptr = session_client_impl.get();
    boca_manager_ = std::make_unique<BocaManager>(
        std::make_unique<boca::OnTaskSessionManager>(nullptr),
        std::move(session_client_impl),
        std::make_unique<boca::BocaSessionManager>(
            session_client_impl_ptr, AccountId::FromUserEmail(kTestEmail)));
  }

  BocaManager* boca_manager() { return boca_manager_.get(); }
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // BocaSessionManager require task_env for mojom binding.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BocaManager> boca_manager_;
};

TEST_F(BocaManagerProducerTest, VerifyOnTaskObserverNotAddedForProducer) {
  ASSERT_TRUE(boca_manager()
                  ->GetBocaSessionManagerForTesting()
                  ->GetObserversForTesting()
                  .empty());
}

class BocaManagerConsumerTest : public testing::Test {
 public:
  BocaManagerConsumerTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {ash::features::kBoca,
                                ash::features::kBocaConsumer},
        /* disabled_features */ {});
    auto session_client_impl =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);
    auto* const session_client_impl_ptr = session_client_impl.get();
    boca_manager_ = std::make_unique<BocaManager>(
        std::make_unique<boca::OnTaskSessionManager>(nullptr),
        std::move(session_client_impl),
        std::make_unique<boca::BocaSessionManager>(
            session_client_impl_ptr, AccountId::FromUserEmail(kTestEmail)));
  }

 protected:
  BocaManager* boca_manager() { return boca_manager_.get(); }
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // BocaSessionManager require task_env for mojom binding.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BocaManager> boca_manager_;
};

TEST_F(BocaManagerConsumerTest, VerifyOnTaskObserverHasAddedForConsumer) {
  ASSERT_FALSE(boca_manager()
                   ->GetBocaSessionManagerForTesting()
                   ->GetObserversForTesting()
                   .empty());
}

}  // namespace
}  // namespace ash
