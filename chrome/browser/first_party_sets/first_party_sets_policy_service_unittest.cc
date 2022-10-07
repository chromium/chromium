// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace first_party_sets {

class MockFirstPartySetsAccessDelegate
    : public network::mojom::FirstPartySetsAccessDelegate {
 public:
  MockFirstPartySetsAccessDelegate() = default;
  MockFirstPartySetsAccessDelegate(const MockFirstPartySetsAccessDelegate&) =
      delete;
  MockFirstPartySetsAccessDelegate& operator=(
      const MockFirstPartySetsAccessDelegate&) = delete;
  ~MockFirstPartySetsAccessDelegate() override = default;

  MOCK_METHOD1(NotifyReady,
               void(network::mojom::FirstPartySetsReadyEventPtr ready_event));
  MOCK_METHOD1(SetEnabled, void(bool));
};

class FirstPartySetsPolicyServiceTest : public testing::Test {
 public:
  FirstPartySetsPolicyServiceTest() {
    // Enable base::Feature for all tests since only the pref can change
    // whether the service is enabled.
    features_.InitAndEnableFeature(features::kFirstPartySets);
  }

  void SetUp() override {
    content::FirstPartySetsHandler::GetInstance()->ResetForTesting();
    mock_delegate_receiver_.Bind(
        mock_delegate_remote_.BindNewPipeAndPassReceiver());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    service_ = FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
        profile_manager_->CreateTestingProfile("TestProfile"));
    ASSERT_NE(service_, nullptr);
    service_->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));
  }

  void TearDown() override {
    DCHECK(service_);
    // Even though we reassign this in SetUp, service may be persisted between
    // tests if the factory has already created a service for the testing
    // profile being used.
    service_->ResetForTesting();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

 protected:
  testing::NiceMock<MockFirstPartySetsAccessDelegate> mock_delegate;
  mojo::Receiver<network::mojom::FirstPartySetsAccessDelegate>
      mock_delegate_receiver_{&mock_delegate};
  mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
      mock_delegate_remote_;

  FirstPartySetsPolicyService* service() { return service_; }

 private:
  content::BrowserTaskEnvironment env_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList features_;
  FirstPartySetsPolicyService* service_;
};

TEST_F(FirstPartySetsPolicyServiceTest,
       OnProfileConfigReady_InitDisabled_NotifiesReadyWithConfig) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config({{test_primary, {test_entry}}});

  service()->OnProfileConfigReady(false, test_config.Clone());

  // Ensure NotifyReady is called with the config.
  base::RunLoop loop;
  network::mojom::FirstPartySetsReadyEventPtr actual;
  EXPECT_CALL(mock_delegate, NotifyReady(_))
      .WillOnce([&](network::mojom::FirstPartySetsReadyEventPtr ptr) {
        actual = std::move(ptr);
        loop.Quit();
      });
  loop.Run();

  EXPECT_FALSE(actual.is_null());
  EXPECT_EQ(actual->config, test_config);
}

TEST_F(FirstPartySetsPolicyServiceTest,
       OnFirstPartySetsEnabledChanged_Default_WithConfig) {
  service()->SetConfigForTesting(net::FirstPartySetsContextConfig());

  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(0);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);
}

TEST_F(FirstPartySetsPolicyServiceTest,
       OnFirstPartySetsEnabledChanged_Default_WithoutConfig) {
  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(0);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);
}

TEST_F(FirstPartySetsPolicyServiceTest,
       OnFirstPartySetsEnabledChanged_Disables_WithConfig) {
  service()->SetConfigForTesting(net::FirstPartySetsContextConfig());
  service()->OnFirstPartySetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);
}

TEST_F(FirstPartySetsPolicyServiceTest,
       OnFirstPartySetsEnabledChanged_Disables_WithoutConfig) {
  service()->OnFirstPartySetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);
}

TEST_F(FirstPartySetsPolicyServiceTest,
       OnFirstPartySetsEnabledChanged_Enables_WithConfig) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config({{test_primary, {test_entry}}});

  service()->SetConfigForTesting(test_config.Clone());
  service()->OnFirstPartySetsEnabledChanged(true);

  // Ensure access delegate is called with SetEnabled(true) and NotifyReady is
  // called with the config.
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  base::RunLoop loop;
  network::mojom::FirstPartySetsReadyEventPtr actual;
  EXPECT_CALL(mock_delegate, NotifyReady(_))
      .WillOnce([&](network::mojom::FirstPartySetsReadyEventPtr ptr) {
        actual = std::move(ptr);
        loop.Quit();
      });
  loop.Run();

  EXPECT_FALSE(actual.is_null());
  EXPECT_EQ(actual->config, test_config);
}

TEST_F(FirstPartySetsPolicyServiceTest,
       OnFirstPartySetsEnabledChanged_Enables_WithoutConfig) {
  service()->OnFirstPartySetsEnabledChanged(true);

  // NotifyReady isn't called since the config isn't ready to be sent.
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);
}

}  // namespace first_party_sets
