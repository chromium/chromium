// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_storage_partition.h"
#include "gmock/gmock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "url/gurl.h"

namespace {

class MockLensSearchController : public lens::TestLensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : lens::TestLensSearchController(tab) {}

  MOCK_METHOD(void,
              OpenLensOverlay,
              (lens::LensOverlayInvocationSource invocation_source),
              (override));

  MOCK_METHOD(void,
              StartContextualization,
              (lens::LensOverlayInvocationSource invocation_source),
              (override));
};

}  // namespace

class ChromeAutocompleteProviderClientTest : public InProcessBrowserTest {
 protected:
  ChromeAutocompleteProviderClientTest() {
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating([](tabs::TabInterface& tab) {
              return std::make_unique<MockLensSearchController>(&tab);
            }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        browser()->profile());
    storage_partition_.set_service_worker_context(&service_worker_context_);
    client_->set_storage_partition(&storage_partition_);
  }

  void TearDownOnMainThread() override {
    client_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  ChromeAutocompleteProviderClient* GetAutocompleteProviderClient() {
    return static_cast<ChromeAutocompleteProviderClient*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->location_bar()
            ->GetOmniboxController()
            ->autocomplete_controller()
            ->autocomplete_provider_client());
  }

  MockLensSearchController* GetLensSearchController() {
    return static_cast<MockLensSearchController*>(
        LensSearchController::From(browser()->GetActiveTabInterface()));
  }

  // Replaces the client with one using an incognito profile. Note that this is
  // a one-way operation. Once a TEST_F calls this, all interactions with
  // |client_| will be off the record.
  void GoOffTheRecord() {
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  }

  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  content::FakeServiceWorkerContext service_worker_context_;

 private:
  content::TestStoragePartition storage_partition_;
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       OpenLensOverlay_Show) {
  EXPECT_CALL(*GetLensSearchController(), OpenLensOverlay(testing::_))
      .Times(1)
      .WillOnce([](lens::LensOverlayInvocationSource invocation_source) {
        EXPECT_EQ(lens::LensOverlayInvocationSource::kOmniboxPageAction,
                  invocation_source);
      });

  GetAutocompleteProviderClient()->OpenLensOverlay(/*show=*/true);
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       OpenLensOverlay_DontShow) {
  EXPECT_CALL(*GetLensSearchController(), StartContextualization(testing::_))
      .Times(1)
      .WillOnce([](lens::LensOverlayInvocationSource invocation_source) {
        EXPECT_EQ(lens::LensOverlayInvocationSource::kOmnibox,
                  invocation_source);
      });

  GetAutocompleteProviderClient()->OpenLensOverlay(/*show=*/false);
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       StartServiceWorker) {
  GURL destination_url("https://google.com/search?q=puppies");

  client_->StartServiceWorker(destination_url);
  EXPECT_TRUE(service_worker_context_
                  .start_service_worker_for_navigation_hint_called());
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       DontStartServiceWorkerInIncognito) {
  GURL destination_url("https://google.com/search?q=puppies");

  GoOffTheRecord();
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       DontStartServiceWorkerIfSuggestDisabled) {
  GURL destination_url("https://google.com/search?q=puppies");

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled,
                                               false);
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}
