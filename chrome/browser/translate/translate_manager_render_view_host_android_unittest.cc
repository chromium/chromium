// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/fake_translate_agent.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/translate/content/android/translate_message.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "url/gurl.h"

namespace translate {
namespace {

class TranslateManagerRenderViewHostAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TranslateManagerRenderViewHostAndroidTest() {}

  TranslateManagerRenderViewHostAndroidTest(
      const TranslateManagerRenderViewHostAndroidTest&) = delete;
  TranslateManagerRenderViewHostAndroidTest& operator=(
      const TranslateManagerRenderViewHostAndroidTest&) = delete;

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
        ->RegisterPage(fake_agent_.BindToNewPageRemote(), details,
                       page_translatable);
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(web_contents());
  }

 protected:
  void SetUp() override {
    // Setup the test environment, including the threads and message loops. This
    // must be done before base::SingleThreadTaskRunner::GetCurrentDefault() is
    // called when setting up the net::TestURLRequestContextGetter below.
    ChromeRenderViewHostTestHarness::SetUp();

    // Clears the translate script so it is fetched every time and sets the
    // expiration delay to a large value by default (in case it was zeroed in a
    // previous test).
    TranslateService::InitializeForTesting(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
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
  std::set<raw_ptr<infobars::InfoBarDelegate, SetExperimental>>
      removed_infobars_;

  FakeTranslateAgent fake_agent_;
};

}  // namespace
}  // namespace translate
