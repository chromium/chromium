// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/keep_alive_dse_policy.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

namespace {

const char* kDSEUrl = "https://foo.com/search?q={searchTerms}";
const char* kNewDSEUrl = "https://bar.com/search?q={searchTerms}";
const char* kNotADSEUrl = "https://not-a-dse.com";

}  // namespace

class KeepAliveDSEPolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  KeepAliveDSEPolicyTest() = default;
  ~KeepAliveDSEPolicyTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    SetContents(CreateTestWebContents());

    // Get the TemplateURLService.
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service_);

    template_url_service_->Load();

    // Create the initial DSE TemplateURLData.
    AddAndSetDSE("foo", "foo_keyword", kDSEUrl);

    //  Check to make sure it's set correctly
    ASSERT_EQ(kDSEUrl,
              template_url_service_->GetDefaultSearchProvider()->url());

    PerformanceManager::GetGraph()->PassToGraph(
        std::make_unique<KeepAliveDSEPolicy>());
  }

  void TearDown() override {
    template_url_service_ = nullptr;
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return TestingProfile::TestingFactory{
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)};
  }

 protected:
  // Helper function to add and set a DSE to the TemplateURLService.
  // Takes the necessary data to create the TemplateURLData and set the DSE.
  void AddAndSetDSE(const std::string& short_name,
                    const std::string& keyword,
                    const std::string& url) {
    // Create the TemplateURLData.
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetKeyword(base::ASCIIToUTF16(keyword));
    data.SetURL(url);

    auto template_url = std::make_unique<TemplateURL>(data);
    template_url_service_->Add(std::move(template_url));

    // Set the newly added TemplateURL as the default search provider.
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        template_url_service_->GetTemplateURLForKeyword(
            base::ASCIIToUTF16(keyword)));
  }

 private:
  PerformanceManagerTestHarnessHelper pm_harness_;
  raw_ptr<TemplateURLService> template_url_service_;
};

// Test that navigating to a non-DSE URL does *not* increment the
// pending reuse refcount.
TEST_F(KeepAliveDSEPolicyTest, NonDSENavigationDoesNotKeepAlive) {
  NavigateAndCommit(GURL(kNotADSEUrl));
  EXPECT_EQ(0, process()->GetPendingReuseRefCountForTesting());
}

// Test that navigating to the current DSE URL *does* increment the
// pending reuse refcount.
TEST_F(KeepAliveDSEPolicyTest, DSENavigationKeepsAlive) {
  NavigateAndCommit(GURL(kDSEUrl));
  EXPECT_EQ(1, process()->GetPendingReuseRefCountForTesting());
}

// Test that changing the DSE releases the keep-alive on the old DSE,
// and that navigating to the *new* DSE URL increments the refcount.
TEST_F(KeepAliveDSEPolicyTest, ChangeDSE) {
  NavigateAndCommit(GURL(kDSEUrl));
  EXPECT_EQ(1, process()->GetPendingReuseRefCountForTesting());

  // Change the default search engine.
  AddAndSetDSE("bar", "bar_keyword", kNewDSEUrl);

  // The original process should no longer be kept alive.
  EXPECT_EQ(0, process()->GetPendingReuseRefCountForTesting());

  NavigateAndCommit(GURL(kNewDSEUrl));
  EXPECT_EQ(1, process()->GetPendingReuseRefCountForTesting());
}

// Test that the DSE is kept alive even after navigating away from it.
TEST_F(KeepAliveDSEPolicyTest, KeepAliveAfterNavigationAway) {
  // Navigate to the DSE page.
  NavigateAndCommit(GURL(kDSEUrl));

  auto* initial_rph = process();
  EXPECT_EQ(1, initial_rph->GetPendingReuseRefCountForTesting());

  // Navigate to a different, non-DSE page.
  NavigateAndCommit(GURL(kNotADSEUrl));

  // The original DSE process *should still* be kept alive.
  EXPECT_EQ(0, process()->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(1, initial_rph->GetPendingReuseRefCountForTesting());
}

}  // namespace performance_manager::policies
