// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_fake_page.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "url/gurl.h"

class TranslateManagerRenderViewHostAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TranslateManagerRenderViewHostAndroidTest() {}

  // Simulates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url,
                          const std::string& lang,
                          bool page_translatable) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
    SimulateOnTranslateLanguageDetermined(lang, page_translatable);
  }

  void SimulateOnTranslateLanguageDetermined(const std::string& lang,
                                             bool page_translatable) {
    translate::LanguageDetectionDetails details;
    details.adopted_language = lang;
    ChromeTranslateClient::FromWebContents(web_contents())
        ->translate_driver()
        ->RegisterPage(fake_page_.BindToNewPageRemote(), details,
                       page_translatable);
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  // Returns the translate infobar if there is 1 infobar and it is a translate
  // infobar.
  translate::TranslateInfoBarDelegate* GetTranslateInfoBar() {
    return (infobar_service()->infobar_count() == 1)
               ? infobar_service()
                     ->infobar_at(0)
                     ->delegate()
                     ->AsTranslateInfoBarDelegate()
               : NULL;
  }

#if !defined(USE_AURA) && !defined(OS_MACOSX)
  // If there is 1 infobar and it is a translate infobar, closes it and returns
  // true.  Returns false otherwise.
  bool CloseTranslateInfoBar() {
    infobars::InfoBarDelegate* infobar = GetTranslateInfoBar();
    if (!infobar)
      return false;
    infobar->InfoBarDismissed();  // Simulates closing the infobar.
    infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
    return true;
  }
#endif

 protected:
  void SetUp() override {
    // Setup the test environment, including the threads and message loops. This
    // must be done before base::ThreadTaskRunnerHandle::Get() is called when
    // setting up the net::TestURLRequestContextGetter below.
    ChromeRenderViewHostTestHarness::SetUp();

    // Clears the translate script so it is fetched every time and sets the
    // expiration delay to a large value by default (in case it was zeroed in a
    // previous test).
    TranslateService::InitializeForTesting(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    InfoBarService::CreateForWebContents(web_contents());
    ChromeTranslateClient::CreateForWebContents(web_contents());
    ChromeTranslateClient::FromWebContents(web_contents())
        ->translate_driver()
        ->set_translate_max_reload_attempts(0);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    TranslateService::ShutdownForTesting();
  }

 private:
  // The infobars that have been removed.
  // WARNING: the pointers point to deleted objects, use only for comparison.
  std::set<infobars::InfoBarDelegate*> removed_infobars_;

  FakePageImpl fake_page_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManagerRenderViewHostAndroidTest);
};

TEST_F(TranslateManagerRenderViewHostAndroidTest,
       ManualTranslateOnReadyBeforeLanguageDetermined) {
  // This only makes sense for infobars, because the check for supported
  // languages moved out of the Infobar into the TranslateManager.
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  GURL url("http://www.google.com");
  // We should not have a translate infobar.
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
  ChromeTranslateClient::FromWebContents(web_contents())
      ->ManualTranslateWhenReady();
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);
  SimulateOnTranslateLanguageDetermined("fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());
}

TEST_F(TranslateManagerRenderViewHostAndroidTest,
       ManualTranslateOnReadyAfterLanguageDetermined) {
  // This only makes sense for infobars, because the check for supported
  // languages moved out of the Infobar into the TranslateManager.
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  GURL url("http://www.google.com");
  // We should not have a translate infobar.
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);
  SimulateOnTranslateLanguageDetermined("fr", true);
  ChromeTranslateClient::FromWebContents(web_contents())
      ->ManualTranslateWhenReady();
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());
}
