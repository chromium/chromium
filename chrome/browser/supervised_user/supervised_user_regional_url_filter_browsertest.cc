// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
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

// The region code for variations service (any should work).
constexpr base::StringPiece kRegionCode = "jp";

// Tests custom filtering logic based on regions, for supervised users.
class SupervisedUserRegionalURLFilterTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SupervisedUserRegionalURLFilterTest() = default;
  ~SupervisedUserRegionalURLFilterTest() override = default;

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
    ASSERT_TRUE(embedded_test_server()->Started());
    std::string host_port = embedded_test_server()->host_port_pair().ToString();

    // Remap all URLs in context of this test to the test server.
    command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                    "MAP *.example.com " + host_port);

    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, kRegionCode);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    kids_chrome_management_client_ =
        static_cast<MockKidsChromeManagementClient*>(
            KidsChromeManagementClientFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    Profile::FromBrowserContext(context),
                    base::BindRepeating(
                        &MockKidsChromeManagementClient::MakeUnique)));
  }

  MockKidsChromeManagementClient* kids_chrome_management_client_;
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};

 private:
  base::CallbackListSubscription create_services_subscription_;
};

// Verifies that the regional setting is passed to the RPC backend.
IN_PROC_BROWSER_TEST_F(SupervisedUserRegionalURLFilterTest, RegionIsAdded) {
  std::string url_to_classify =
      "http://www.example.com/simple.html";  // The hostname must be handled by
                                             // embedded server, see {@link
                                             // SetUpCommandLine}.

  ClassifyUrlRequest expected;
  expected.set_region_code(std::string(kRegionCode));
  expected.set_url(url_to_classify);
  EXPECT_CALL(*kids_chrome_management_client_,
              ClassifyURL(Pointee(EqualsProto(expected)), /* callback= */ _));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url_to_classify)));
}

}  // namespace
