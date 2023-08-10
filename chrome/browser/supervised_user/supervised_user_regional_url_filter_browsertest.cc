// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/features.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

using ::kids_chrome_management::ClassifyUrlRequest;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Pointee;

// Surprisingly, we don't have proto-comparators from gtest available. Remove
// once they're available.
MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

using TestCase = std::tuple<supervised_user::SupervisionMixin::SignInMode,
                            std::vector<base::test::FeatureRef>>;

// The region code for variations service (any should work).
constexpr base::StringPiece kRegionCode = "jp";

// Tests custom filtering logic based on regions, for supervised users.
class SupervisedUserRegionalURLFilterTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<TestCase> {
 public:
  SupervisedUserRegionalURLFilterTest() {
    // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitWithFeatures(
        GetEnabledFeatures(),
        /*disabled_features=*/{features::kHttpsUpgrades});
    supervision_mixin_.InitFeatures();
  }

 protected:
  class MockKidsChromeManagementClient : public KidsChromeManagementClient {
   public:
    explicit MockKidsChromeManagementClient(Profile* profile)
        : KidsChromeManagementClient(
              profile->GetDefaultStoragePartition()
                  ->GetURLLoaderFactoryForBrowserProcess(),
              IdentityManagerFactory::GetForProfile(profile)) {
      // Without forwarding the call to the real implementation, the browser
      // hangs and the test times out.
      ON_CALL(*this, ClassifyURL)
          .WillByDefault(
              [this](std::unique_ptr<ClassifyUrlRequest> request_proto,
                     ::KidsChromeManagementClient::KidsChromeManagementCallback
                         callback) {
                KidsChromeManagementClient::ClassifyURL(
                    std::move(request_proto), std::move(callback));
              });
    }

    MOCK_METHOD(
        void,
        ClassifyURL,
        (std::unique_ptr<ClassifyUrlRequest> request_proto,
         ::KidsChromeManagementClient::KidsChromeManagementCallback callback),
        (override));

    static std::unique_ptr<KeyedService> MakeUnique(
        content::BrowserContext* context) {
      return std::make_unique<NiceMock<MockKidsChromeManagementClient>>(
          static_cast<Profile*>(context));
    }
  };

  supervised_user::SupervisionMixin::SignInMode GetSignInMode() const {
    return std::get<0>(GetParam());
  }
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const {
    return std::get<1>(GetParam());
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SupervisedUserRegionalURLFilterTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, kRegionCode);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    KidsChromeManagementClientFactory::GetInstance()->SetTestingFactoryAndUse(
        Profile::FromBrowserContext(context),
        base::BindRepeating(&MockKidsChromeManagementClient::MakeUnique));
  }

  MockKidsChromeManagementClient* GetKidsChromeManagementClient() {
    return static_cast<MockKidsChromeManagementClient*>(
        KidsChromeManagementClientFactory::GetForProfile(browser()->profile()));
  }

  // Only supervised users have their url requests classified, only when the
  // feature is enabled.
  bool ShouldUrlsBeClassified() const {
    if (GetSignInMode() !=
        supervised_user::SupervisionMixin::SignInMode::kSupervised) {
      return false;
    }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
    return true;
#else
    return base::FeatureList::IsEnabled(
        supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {
          .sign_in_mode = GetSignInMode(),
          .embedded_test_server_options = {.resolver_rules_map_host_list =
                                               "*.example.com"},
      }};
};

// Verifies that the regional setting is passed to the RPC backend.
IN_PROC_BROWSER_TEST_P(SupervisedUserRegionalURLFilterTest, RegionIsAdded) {
  std::string url_to_classify =
      "http://www.example.com/simple.html";  // The hostname must be handled by
                                             // embedded server - see
                                             // supervision_mixin_
                                             // configuration.

  ClassifyUrlRequest expected;
  expected.set_region_code(std::string(kRegionCode));
  expected.set_url(url_to_classify);

  int number_of_expected_calls = ShouldUrlsBeClassified() ? 1 : 0;

  EXPECT_CALL(*GetKidsChromeManagementClient(),
              ClassifyURL(Pointee(EqualsProto(expected)), /*callback=*/_))
      .Times(number_of_expected_calls);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url_to_classify)));
}

// Instead of /0, /1... print human-readable description of the test: type of
// the user signed in and the list of conditionally enabled features.
std::string PrettyPrintTestCaseName(
    const ::testing::TestParamInfo<TestCase>& info) {
  supervised_user::SupervisionMixin::SignInMode sign_in_mode =
      std::get<0>(info.param);
  std::vector<base::test::FeatureRef> features = std::get<1>(info.param);
  std::vector<std::string> feature_names(features.size());
  base::ranges::transform(
      features, feature_names.begin(),
      [](const base::test::FeatureRef& ref) { return ref->name; });

  std::stringstream ss;
  ss << sign_in_mode << "AccountWith"
     << (feature_names.empty() ? "NoFeatures"
                               : base::JoinString(feature_names, "And"));
  return ss.str();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserRegionalURLFilterTest,
    testing::Combine(
        testing::Values(
#if !BUILDFLAG(IS_CHROMEOS_ASH)
            // Only for platforms that support signed-out browser.
            supervised_user::SupervisionMixin::SignInMode::kSignedOut,
#endif
            supervised_user::SupervisionMixin::SignInMode::kRegular,
            supervised_user::SupervisionMixin::SignInMode::kSupervised),
        testing::Values(
            std::vector<base::test::FeatureRef>(),
            std::vector<base::test::FeatureRef>(
                {supervised_user::
                     kFilterWebsitesForSupervisedUsersOnDesktopAndIOS}))),
    &PrettyPrintTestCaseName);
}  // namespace
