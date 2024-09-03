// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_app_defined_websites.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AppDefinedWebsitesTest : public testing::Test {
 public:
  AppDefinedWebsitesTest()
      : unit_under_test_(AppDefinedWebsites(
            base::BindRepeating(&AppDefinedWebsitesTest::DomainProvider,
                                base::Unretained(this)))) {}

  AppDefinedWebsitesTest(const AppDefinedWebsitesTest&) = delete;
  AppDefinedWebsitesTest& operator=(const AppDefinedWebsitesTest&) = delete;
  ~AppDefinedWebsitesTest() override = default;

  void GetAppDefinedDomains(AppDefinedDomainCriteria criteria,
                            AppDefinedWebsites::AppDomainCallback callback) {
    unit_under_test_.GetAppDefinedDomains(criteria, std::move(callback));
  }

  // Call GetAppDefinedDomains and block until the `callback` has been executed.
  void GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria criteria,
      AppDefinedWebsites::AppDomainCallback callback) {
    base::RunLoop runloop;
    GetAppDefinedDomains(
        criteria, base::BindOnce(
                      [](base::OnceClosure runloop_callback,
                         AppDefinedWebsites::AppDomainCallback result_callback,
                         const std::vector<std::string>& domains) {
                        std::move(result_callback).Run(domains);
                        std::move(runloop_callback).Run();
                      },
                      runloop.QuitClosure(), std::move(callback)));
    runloop.Run();
  }

  void SetProviderResult(AppDefinedDomainCriteria criteria,
                         std::vector<std::string> results) {
    provider_results_[criteria] = std::move(results);
  }

  base::TestWaitableEvent* WaitForSignalBeforeProvidingDomains(
      AppDefinedDomainCriteria criteria) {
    if (!provider_waits_.contains(criteria)) {
      provider_waits_[criteria] = std::make_unique<base::TestWaitableEvent>(
          base::WaitableEvent::ResetPolicy::AUTOMATIC);
    }
    return provider_waits_[criteria].get();
  }

  int GetProviderCallCount(AppDefinedDomainCriteria criteria) {
    return provider_call_counts_[criteria];
  }

 private:
  std::vector<std::string> DomainProvider(AppDefinedDomainCriteria criteria) {
    if (provider_waits_.contains(criteria)) {
      provider_waits_[criteria]->Wait();
    }
    provider_call_counts_[criteria]++;
    auto find_it = provider_results_.find(criteria);
    if (find_it == provider_results_.end()) {
      return {};
    }
    return find_it->second;
  }

  base::test::TaskEnvironment task_environment_;

  // These maps are access both from background threads and the main test
  // thread. Test cases should not attempt to modify these while interacting
  // with the unit under test.
  base::flat_map<AppDefinedDomainCriteria, int> provider_call_counts_;
  base::flat_map<AppDefinedDomainCriteria, std::vector<std::string>>
      provider_results_;

  base::flat_map<AppDefinedDomainCriteria,
                 std::unique_ptr<base::TestWaitableEvent>>
      provider_waits_;

  AppDefinedWebsites unit_under_test_;
};

namespace {
TEST_F(AppDefinedWebsitesTest, ProvidedDomainsAreReturned) {
  // Since the test fixture is responsible for providing the results, this test
  // is as much a validation test for the test fixture itself.
  SetProviderResult(AppDefinedDomainCriteria::kAndroidAssetStatements,
                    {"asset-statement.example"});
  GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria::kAndroidAssetStatements,
      base::BindOnce([](const std::vector<std::string>& domains) {
        EXPECT_THAT(domains, testing::ElementsAre("asset-statement.example"));
      }));

  SetProviderResult(AppDefinedDomainCriteria::kAndroidVerifiedAppLinks,
                    {"verified-app-link.example"});
  GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria::kAndroidVerifiedAppLinks,
      base::BindOnce([](const std::vector<std::string>& domains) {
        EXPECT_THAT(domains, testing::ElementsAre("verified-app-link.example"));
      }));

  SetProviderResult(AppDefinedDomainCriteria::kAndroidWebLinks,
                    {"android-web-link.example"});
  GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria::kAndroidWebLinks,
      base::BindOnce([](const std::vector<std::string>& domains) {
        EXPECT_THAT(domains, testing::ElementsAre("android-web-link.example"));
      }));

  SetProviderResult(
      AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks,
      {"asset-statement.example", "android-web-link.example"});
  GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks,
      base::BindOnce([](const std::vector<std::string>& domains) {
        EXPECT_THAT(domains, testing::ElementsAre("asset-statement.example",
                                                  "android-web-link.example"));
      }));
}

TEST_F(AppDefinedWebsitesTest, RepeatedCallsAreCached) {
  SetProviderResult(AppDefinedDomainCriteria::kAndroidAssetStatements,
                    {"asset-statement.example"});

  GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria::kAndroidAssetStatements,
      base::BindOnce([](const std::vector<std::string>& domains) {
        EXPECT_THAT(domains, testing::ElementsAre("asset-statement.example"));
      }));
  // We expect the provider to have been called once.
  EXPECT_EQ(1, GetProviderCallCount(
                   AppDefinedDomainCriteria::kAndroidAssetStatements));

  // Try to get the same list of domains a second time and observe that the
  // provider is not called again.
  GetAppDefinedDomainsSync(
      AppDefinedDomainCriteria::kAndroidAssetStatements,
      base::BindOnce([](const std::vector<std::string>& domains) {
        EXPECT_THAT(domains, testing::ElementsAre("asset-statement.example"));
      }));
  EXPECT_EQ(1, GetProviderCallCount(
                   AppDefinedDomainCriteria::kAndroidAssetStatements));
}

TEST_F(AppDefinedWebsitesTest, RacingCallsAreNotDuplicated) {
  SetProviderResult(AppDefinedDomainCriteria::kAndroidAssetStatements,
                    {"asset-statement.example"});
  // Set up a runloop and a barrier that waits for both calls to have their
  // callbacks invoked.
  base::RunLoop runloop;
  base::RepeatingClosure done_closure =
      base::BarrierClosure(2, runloop.QuitClosure());
  base::TestWaitableEvent* provider_trigger =
      WaitForSignalBeforeProvidingDomains(
          AppDefinedDomainCriteria::kAndroidAssetStatements);

  // Call twice right after each other to get a race
  GetAppDefinedDomains(
      AppDefinedDomainCriteria::kAndroidAssetStatements,
      base::BindOnce(
          [](base::OnceClosure on_done,
             const std::vector<std::string>& domains) {
            EXPECT_THAT(domains,
                        testing::ElementsAre("asset-statement.example"));
            std::move(on_done).Run();
          },
          done_closure));
  GetAppDefinedDomains(
      AppDefinedDomainCriteria::kAndroidAssetStatements,
      base::BindOnce(
          [](base::OnceClosure on_done,
             const std::vector<std::string>& domains) {
            EXPECT_THAT(domains,
                        testing::ElementsAre("asset-statement.example"));
            std::move(on_done).Run();
          },
          done_closure));

  // Allow the provider to run.
  provider_trigger->Signal();

  runloop.Run();
  EXPECT_EQ(1, GetProviderCallCount(
                   AppDefinedDomainCriteria::kAndroidAssetStatements));
}

}  // namespace
}  // namespace android_webview
