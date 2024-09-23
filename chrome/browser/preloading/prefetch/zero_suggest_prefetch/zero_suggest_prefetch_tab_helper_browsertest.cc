// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      int provider_types)
      : AutocompleteController(std::move(provider_client), provider_types) {}
  ~MockAutocompleteController() override = default;
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;

  // AutocompleteController:
  MOCK_METHOD1(Start, void(const AutocompleteInput&));
  MOCK_METHOD1(StartPrefetch, void(const AutocompleteInput&));
};

}  // namespace

class ZeroSuggestPrefetchTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    auto client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_template_url_service(template_url_service);

    auto controller =
        std::make_unique<testing::NiceMock<MockAutocompleteController>>(
            std::move(client_), 0);
    controller_ = controller.get();
    browser()
        ->window()
        ->GetLocationBar()
        ->GetOmniboxView()
        ->controller()
        ->SetAutocompleteControllerForTesting(std::move(controller));
  }

  base::test::ScopedFeatureList feature_list_;
  raw_ptr<testing::NiceMock<MockAutocompleteController>,
          AcrossTasksDanglingUntriaged>
      controller_;
};

class ZeroSuggestPrefetchTabHelperBrowserTestOnNTP
    : public ZeroSuggestPrefetchTabHelperBrowserTest {
 public:
  ZeroSuggestPrefetchTabHelperBrowserTestOnNTP() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kZeroSuggestPrefetching,
                              omnibox::kOmniboxOnClobberFocusTypeOnContent},
        /*disabled_features=*/{omnibox::kZeroSuggestPrefetchingOnSRP,
                               omnibox::kZeroSuggestPrefetchingOnWeb});
  }
};

class ZeroSuggestPrefetchTabHelperBrowserTestOnSRP
    : public ZeroSuggestPrefetchTabHelperBrowserTest {
 public:
  ZeroSuggestPrefetchTabHelperBrowserTestOnSRP() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnSRP,
                              omnibox::kOmniboxOnClobberFocusTypeOnContent},
        /*disabled_features=*/{omnibox::kZeroSuggestPrefetching,
                               omnibox::kZeroSuggestPrefetchingOnWeb});
  }
};

class ZeroSuggestPrefetchTabHelperBrowserTestOnWeb
    : public ZeroSuggestPrefetchTabHelperBrowserTest {
 public:
  ZeroSuggestPrefetchTabHelperBrowserTestOnWeb() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnWeb,
                              omnibox::kOmniboxOnClobberFocusTypeOnContent},
        /*disabled_features=*/{omnibox::kZeroSuggestPrefetching,
                               omnibox::kZeroSuggestPrefetchingOnSRP});
  }
};

// Tests that navigating to or switching to the NTP starts a prefetch request
// with the expected page classification.
IN_PROC_BROWSER_TEST_F(ZeroSuggestPrefetchTabHelperBrowserTestOnNTP,
                       StartPrefetch) {
  const std::string srp_url = "https://www.google.com/search?q=hello+world";
  const std::string web_url = "https://www.example.com";
  auto input_is_correct = [](const AutocompleteInput& input) {
    return input.current_page_classification() ==
               metrics::OmniboxEventProto::NTP_ZPS_PREFETCH &&
           input.focus_type() == metrics::OmniboxFocusType::INTERACTION_FOCUS;
  };

  {
    // Navigating to the NTP in the current tab triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabPageURL),
        WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(1, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground NTP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabPageURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground SRP does not trigger prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(srp_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(3, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground Web page does not trigger prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(web_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_EQ(4, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Switching to an NTP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    browser()->tab_strip_model()->ActivateTabAt(1);

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
}

// Tests that navigating to or switching to the SRP starts a prefetch request
// with the expected page classification.
IN_PROC_BROWSER_TEST_F(ZeroSuggestPrefetchTabHelperBrowserTestOnSRP,
                       StartPrefetch) {
  const std::string srp_url = "https://www.google.com/search?q=hello+world";
  const std::string web_url = "https://www.example.com";
  auto input_is_correct = [](const AutocompleteInput& input) {
    return input.current_page_classification() ==
               metrics::OmniboxEventProto::SRP_ZPS_PREFETCH &&
           input.focus_type() == metrics::OmniboxFocusType::INTERACTION_CLOBBER;
  };

  {
    // Navigating to the SRP in the current tab triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(srp_url), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(1, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a background SRP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(srp_url), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground SRP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(srp_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(3, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground Web page does not trigger prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(web_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_EQ(4, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Switching to an SRP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    browser()->tab_strip_model()->ActivateTabAt(1);

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
}

// Tests that navigating to or switching to a Web URL (non-NTP/non-SRP) starts a
// prefetch request with the expected page classification.
IN_PROC_BROWSER_TEST_F(ZeroSuggestPrefetchTabHelperBrowserTestOnWeb,
                       StartPrefetch) {
  const std::string srp_url = "https://www.google.com/search?q=hello+world";
  const std::string web_url = "https://www.example.com";
  auto input_is_correct = [](const AutocompleteInput& input) {
    return input.current_page_classification() ==
               metrics::OmniboxEventProto::OTHER_ZPS_PREFETCH &&
           input.focus_type() == metrics::OmniboxFocusType::INTERACTION_CLOBBER;
  };

  {
    // Navigating to a Web page in the current tab triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(web_url), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(1, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a background Web page triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(web_url), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground Web page triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(web_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(3, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground SRP does not trigger prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(srp_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(4, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Switching to a Web tab triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch(testing::Truly(input_is_correct)))
        .Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    browser()->tab_strip_model()->ActivateTabAt(1);

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
}
