// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/language/accept_languages_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace translate {
namespace {

static const char kTestValidScript[] =
    "var google = {};"
    "window.isTranslationRestored = false;"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return true;"
    "        },"
    "        restore : function() {"
    "          isTranslationRestored = true;"
    "          return;"
    "        },"
    "        getDetectedLanguage : function() {"
    "          return \"fr\";"
    "        },"
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          var error = (sourceLang == 'auto') ? true : false;"
    "          onTranslateProgress(100, true, error);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptInitializationError[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return error;"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptIdenticalLanguages[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return true;"
    "        },"
    "        restore : function() {"
    "          return;"
    "        },"
    "        getDetectedLanguage : function() {"
    "          return \"en\";"
    "        },"
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          onTranslateProgress(100, true, 0);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptAvailableTimeout[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return false;"
    "        },"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptUnexpectedScriptError[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return true;"
    "        },"
    "        restore : function() {"
    "          return;"
    "        },"
    "        getDetectedLanguage : function() {"
    "          return \"fr\";"
    "        },"
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          return error;"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptBadOrigin[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return true;"
    "        },"
    "        restore : function() {"
    "          return;"
    "        },"
    "        getDetectedLanguage : function() {"
    "          return \"fr\";"
    "        },"
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          var url = \"\";"
    "          cr.googleTranslate.onLoadJavascript(url);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptLoadError[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return true;"
    "        },"
    "        restore : function() {"
    "          return;"
    "        },"
    "        getDetectedLanguage : function() {"
    "          return \"fr\";"
    "        },"
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          var url = \"https://translate.googleapis.com/INVALID\";"
    "          cr.googleTranslate.onLoadJavascript(url);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTranslateHrefHintStatusHistogram[] =
    "Translate.HrefHint.Status";

class TranslateManagerBrowserTest : public InProcessBrowserTest {
 public:
  TranslateManagerBrowserTest() {
    error_subscription_ = TranslateManager::RegisterTranslateErrorCallback(
        base::BindRepeating(&TranslateManagerBrowserTest::OnTranslateError,
                            base::Unretained(this)));
  }

  TranslateManagerBrowserTest(const TranslateManagerBrowserTest&) = delete;
  TranslateManagerBrowserTest& operator=(const TranslateManagerBrowserTest&) =
      delete;

  ~TranslateManagerBrowserTest() override = default;

  void WaitUntilLanguageDetermined(
      ChromeTranslateClient* chrome_translate_client) {
    if (chrome_translate_client->GetLanguageState().source_language().empty())
      language_determined_waiter_->Wait();
  }

  void WaitUntilPageTranslated() {
    translate::CreateTranslateWaiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        TranslateWaiter::WaitEvent::kPageTranslated)
        ->Wait();
  }

  void ResetObserver() {
    language_determined_waiter_ = translate::CreateTranslateWaiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        TranslateWaiter::WaitEvent::kLanguageDetermined);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != "/mock_translate_script.js")
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(script_);
    http_response->set_content_type("text/javascript");
    return std::move(http_response);
  }

  void OnTranslateError(const TranslateErrorDetails& details) {
    error_type_ = details.error;
  }

  TranslateErrors GetPageTranslatedResult() { return error_type_; }

  ChromeTranslateClient* GetChromeTranslateClient() {
    return ChromeTranslateClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void ClickFrenchHrefTranslateLinkOnGooglePage() {
    SetTranslateScript(kTestValidScript);

    ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

    // Load a page with hrefTranslate tags.
    ASSERT_TRUE(
        AddTabAtIndex(0,
                      GURL(embedded_test_server()->GetURL(
                          "www.google.com", "/href_translate_test.html")),
                      ui::PAGE_TRANSITION_TYPED));
    ResetObserver();
    chrome_translate_client = GetChromeTranslateClient();
    WaitUntilLanguageDetermined(chrome_translate_client);

    // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
    // language detection. All pages are detected as "fr".
    //
    // In the case of href translate, we don't actually care if the current
    // page is french, only that it loaded and whether href translate
    // updates the current language state or not.
    EXPECT_EQ("fr",
              chrome_translate_client->GetLanguageState().source_language());

    // Navigate to the French page by way of a link on the original page
    ResetObserver();
    chrome_translate_client->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage("");
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);

    const std::string click_link_js =
        "(function() { document.getElementById('test').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_link_js));

    // Detect language on the new page
    // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
    // language detection. All pages are currently detected as "fr" due to the
    // override.
    WaitUntilLanguageDetermined(chrome_translate_client);
    EXPECT_EQ("fr",
              chrome_translate_client->GetLanguageState().source_language());
  }

 protected:
  // InProcessBrowserTest members.
  void SetUp() override { InProcessBrowserTest::SetUp(); }
  void SetUpOnMainThread() override {
    ResetObserver();
    error_type_ = TranslateErrors::NONE;

    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &TranslateManagerBrowserTest::HandleRequest, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    GetChromeTranslateClient()->GetTranslatePrefs()->ResetToDefaults();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Enable Experimental web platform features for HrefTranslate tests
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalWebPlatformFeatures);

    command_line->AppendSwitchASCII(
        switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
    // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
    // language detection.
    // All pages will have language detected as "fr". These tests are around
    // the manager logic so the language detection behavior should be
    // deterministic rather than relying on the page content.
    command_line->AppendSwitch(::switches::kOverrideLanguageDetection);
  }
  void TearDownOnMainThread() override {
    language_determined_waiter_.reset();

    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetTranslateScript(const std::string& script) { script_ = script; }

 private:
  TranslateErrors error_type_;

  base::CallbackListSubscription error_subscription_;

  std::unique_ptr<TranslateWaiter> language_determined_waiter_;

  std::string script_;
};

// Tests that language detection returns a response.
// TODO(crbug.com/40200965): Migrate to better mechanism for testing around
// language detection. Seeding the TFLite model can racy/flaky on browsertests
// so we override the response.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageLanguageDetection) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());
}

// Tests that the language detection / HTML attribute override works correctly.
// For languages in the always-translate list, the detected language should
// override the HTML attribute. For all other languages, the HTML attribute
// should be used.
//
// TODO(crbug.com/40200965): Migrate to better mechanism for testing around
// language detection. All pages will return "fr" as the detected language.
//
// Disabled due to language detection always returning French. (See TODO)
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       DISABLED_PageLanguageDetectionConflict) {
  // Open a new tab with a page in Korean with incorrect HTML language
  // attribute specified. The language attribute should not be overridden by the
  // language detection.
  ASSERT_TRUE(AddTabAtIndex(
      0,
      GURL(embedded_test_server()->GetURL("/korean_page_lang_conflict.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().source_language());
}

// Tests that the language detection / HTML attribute override works correctly.
// For languages in the always-translate list, the detected language should
// override the HTML attribute. For all other languages, the HTML attribute
// should be used.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageLanguageDetectionConflictOverride) {
  // Open a new tab with a page in French with incorrect HTML language
  // attribute specified. The language attribute should be overridden by the
  // language detection.
  ASSERT_TRUE(AddTabAtIndex(
      0,
      GURL(embedded_test_server()->GetURL("/french_page_lang_conflict.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());
}

// Test that the translation was successful.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationSuccess) {
  SetTranslateScript(kTestValidScript);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  base::HistogramTester histograms;

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();

  LanguageState* language_state = manager->GetLanguageState();
  EXPECT_EQ(TranslationType::kUninitialized,
            language_state->translation_type());

  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en", true,
      TranslationType::kAutomaticTranslationByPref);

  EXPECT_EQ(TranslationType::kAutomaticTranslationByPref,
            language_state->translation_type());
  WaitUntilPageTranslated();

  EXPECT_FALSE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::NONE, GetPageTranslatedResult());
  EXPECT_EQ(TranslationType::kUninitialized,
            language_state->translation_type());

  histograms.ExpectTotalCount("Translate.LanguageDetection.ContentLength", 1);
  histograms.ExpectBucketCount("Translate.LanguageDetection.ContentLength", 149,
                               1);
  histograms.ExpectTotalCount("Translate.LanguageDeterminedDuration", 1);
}

// Test that the translation was successful in an about:blank page.
// This is a regression test for https://crbug.com/943685.
// Disabled due to flakiness: https://crbug.com/1202065.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       DISABLED_PageTranslationAboutBlank) {
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();

  // Open a pop-up window and leave it at the initial about:blank URL.
  content::WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.open('about:blank', 'popup')"));
  content::WebContents* popup = popup_observer.GetWebContents();

  // A round-trip to the renderer process helps avoid a race where the
  // browser-side translate structures are not yet ready for the translate call.
  EXPECT_EQ("ping", content::EvalJs(popup, "'ping'"));

  // Translate the about:blank page.
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(popup);
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage("fr", "en", true);

  // Verify that the crash from https://crbug.com/943685 didn't happen.
  EXPECT_EQ("still alive", content::EvalJs(popup, "'still alive'"));

  // Wait for translation to finish and verify it was successful.
  WaitUntilPageTranslated();
  EXPECT_FALSE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::NONE, GetPageTranslatedResult());
}

// Test that hrefTranslate is propagating properly.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateSuccess) {
  base::HistogramTester histograms;
  GetChromeTranslateClient()
      ->GetTranslateManager()
      ->SetIgnoreMissingKeyForTesting(true);

  ClickFrenchHrefTranslateLinkOnGooglePage();

  // See that the page was translated automatically.
  WaitUntilPageTranslated();
  EXPECT_EQ("ja",
            GetChromeTranslateClient()->GetLanguageState().current_language());

  // The target shouldn't be added to accept languages.
  EXPECT_FALSE(
      AcceptLanguagesServiceFactory::GetForBrowserContext(browser()->profile())
          ->IsAcceptLanguage("ja"));

  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(
          TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated),
      1);
}

// Test that hrefTranslate doesn't auto-translate if the originator of the
// navigation isn't a Google origin.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       HrefTranslateNotFromGoogle) {
  base::HistogramTester histograms;
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // Load a page with hrefTranslate tags.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/href_translate_test.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection. All pages will return "fr" as the detected language.
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Navigate to the French page by way of a link on the original page.
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_link_js));

  // Detect language on the new page.
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection. Note: this only tests that the source language was
  // whatever the page was before. The real test is that the href translate
  // update did not occur, tested by AutoTranslateTo() below and the histograms.
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  EXPECT_EQ("", chrome_translate_client->GetLanguageState().AutoTranslateTo());

  histograms.ExpectTotalCount(kTranslateHrefHintStatusHistogram, 0);
}

// Test that hrefTranslate with an unsupported language doesn't trigger.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateUnsupported) {
  base::HistogramTester histograms;
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // Load a page with hrefTranslate tags.
  ASSERT_TRUE(AddTabAtIndex(0,
                            GURL(embedded_test_server()->GetURL(
                                "www.google.com", "/href_translate_test.html")),
                            ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Navigate to the French page by way of a link on the original page. This
  // link has the hrefTranslate attribute set to "unsupported", so it shouldn't
  // trigger translate.
  ResetObserver();
  chrome_translate_client->GetTranslateManager()
      ->GetLanguageState()
      ->SetSourceLanguage("");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { "
      "document.getElementById('test-unsupported-language').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_link_js));

  // Detect language on the new page.
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection. Note: this only tests that the source language was
  // whatever the page was before. The real test is that the href translate
  // update did not occur, tested by AutoTranslateTo() below and the histograms.
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  EXPECT_EQ("", chrome_translate_client->GetLanguageState().AutoTranslateTo());

  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(TranslateBrowserMetrics::HrefTranslateStatus::
                           kNoUiShownNotAutoTranslated),
      1);
}

// Test an href translate link to a conflicted page.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateConflict) {
  base::HistogramTester histograms;
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);

  SetTranslateScript(kTestValidScript);

  // Load a page with hrefTranslate tags.
  ASSERT_TRUE(AddTabAtIndex(0,
                            GURL(embedded_test_server()->GetURL(
                                "www.google.com", "/href_translate_test.html")),
                            ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection. All pages will return "fr" as the detected language.
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Navigate to the French page that thinks its in English by way of a link on
  // the original page.
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test-conflict').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_link_js));

  // Detect language on the new page.
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // See that the page was translated automatically.
  WaitUntilPageTranslated();
  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().current_language());

  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(
          TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated),
      1);
}

// Test an href translate link without an href lang for the landing page.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateNoHrefLang) {
  base::HistogramTester histograms;
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // Load a page with hrefTranslate tags.
  ASSERT_TRUE(AddTabAtIndex(0,
                            GURL(embedded_test_server()->GetURL(
                                "www.google.com", "/href_translate_test.html")),
                            ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection. All pages will return "fr" as the detected language.
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Use a link with no hrefLang to navigate to a French page.
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test-no-hrefLang').click(); "
      "})();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // See that the page was translated automatically
  WaitUntilPageTranslated();
  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().current_language());

  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(
          TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated),
      1);
}

// Test an href translate link that's overridden by the auto translate settings.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       HrefTranslateOverridenByAutoTranslate) {
  base::HistogramTester histograms;
  GetChromeTranslateClient()
      ->GetTranslateManager()
      ->SetIgnoreMissingKeyForTesting(true);

  // Before browsing: set auto translate from French to Chinese.
  GetChromeTranslateClient()
      ->GetTranslatePrefs()
      ->AddLanguagePairToAlwaysTranslateList("fr", "zh-CN");

  ClickFrenchHrefTranslateLinkOnGooglePage();

  // See that the page was translated automatically.
  WaitUntilPageTranslated();
  EXPECT_EQ("zh-CN",
            GetChromeTranslateClient()->GetLanguageState().current_language());

  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(TranslateBrowserMetrics::HrefTranslateStatus::
                           kAutoTranslatedDifferentTargetLanguage),
      1);
}

// Test that hrefTranslate will auto translate if the target language is on the
// user's language blocklist, since that blocklist will be overridden.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       HrefTranslateLanguageBlocked) {
  base::HistogramTester histograms;
  GetChromeTranslateClient()
      ->GetTranslateManager()
      ->SetIgnoreMissingKeyForTesting(true);
  GetChromeTranslateClient()->GetTranslatePrefs()->AddToLanguageList("fr",
                                                                     true);

  ClickFrenchHrefTranslateLinkOnGooglePage();

  // See that the page was translated automatically.
  WaitUntilPageTranslated();
  EXPECT_EQ("ja",
            GetChromeTranslateClient()->GetLanguageState().current_language());

  // The target shouldn't be added to accept languages.
  EXPECT_FALSE(
      AcceptLanguagesServiceFactory::GetForBrowserContext(browser()->profile())
          ->IsAcceptLanguage("ja"));

  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(
          TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated),
      1);
}

// Test that hrefTranslate doesn't translate if the website is in the user's
// site blocklist.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateSiteBlocked) {
  base::HistogramTester histograms;
  GetChromeTranslateClient()
      ->GetTranslateManager()
      ->SetIgnoreMissingKeyForTesting(true);
  GetChromeTranslateClient()->GetTranslatePrefs()->AddSiteToNeverPromptList(
      "www.google.com");

  ClickFrenchHrefTranslateLinkOnGooglePage();

  // The page should not have been automatically translated.
  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(TranslateBrowserMetrics::HrefTranslateStatus::
                           kNoUiShownNotAutoTranslated),
      1);
}

// Test that hrefTranslate doesn't translate if the language is in the user's
// language blocklist and the website is in the user's site blocklist.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       HrefTranslateLanguageAndSiteBlocked) {
  base::HistogramTester histograms;
  GetChromeTranslateClient()
      ->GetTranslateManager()
      ->SetIgnoreMissingKeyForTesting(true);
  GetChromeTranslateClient()->GetTranslatePrefs()->AddToLanguageList("fr",
                                                                     true);
  GetChromeTranslateClient()->GetTranslatePrefs()->AddSiteToNeverPromptList(
      "www.google.com");

  ClickFrenchHrefTranslateLinkOnGooglePage();

  // The page should not have been automatically translated.
  histograms.ExpectUniqueSample(
      kTranslateHrefHintStatusHistogram,
      static_cast<int>(TranslateBrowserMetrics::HrefTranslateStatus::
                           kNoUiShownNotAutoTranslated),
      1);
}

// Test if there was an error during translation.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationError) {
  SetTranslateScript(kTestValidScript);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French and translate to French to force an
  // error.
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection. All pages will return "fr" as the detected language.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "fr",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::TRANSLATION_ERROR, GetPageTranslatedResult());
}

// Test if there was an error during translate library initialization.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationInitializationError) {
  SetTranslateScript(kTestScriptInitializationError);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::INITIALIZATION_ERROR, GetPageTranslatedResult());
}

// Test the checks translate lib never gets ready and throws timeout.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationTimeoutError) {
  SetTranslateScript(kTestScriptAvailableTimeout);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::TRANSLATION_TIMEOUT, GetPageTranslatedResult());
}

// Test the checks if both source and target languages mentioned are identical.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationIdenticalLanguagesError) {
  SetTranslateScript(kTestScriptIdenticalLanguages);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage("aa", "en", true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::IDENTICAL_LANGUAGES, GetPageTranslatedResult());
}

// Test if there was an error during translatePage script execution.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationUnexpectedScriptError) {
  SetTranslateScript(kTestScriptUnexpectedScriptError);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::UNEXPECTED_SCRIPT_ERROR,
            GetPageTranslatedResult());
}

// Test if securityOrigin mentioned in url is valid.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationBadOriginError) {
  SetTranslateScript(kTestScriptBadOrigin);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::BAD_ORIGIN, GetPageTranslatedResult());
}

// Test if there was an error during script load.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationScriptLoadError) {
  SetTranslateScript(kTestScriptLoadError);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::SCRIPT_LOAD_ERROR, GetPageTranslatedResult());
}

// Test that session restore restores the translate infobar and other translate
// settings.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PRE_TranslateSessionRestore) {
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  ResetObserver();

  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), french_url));

  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());
}

IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, TranslateSessionRestore) {
  // Make restored tab active to (on some platforms) initiate language
  // detection.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  content::WebContents* restored_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeTranslateClient* restored_translate_client =
      ChromeTranslateClient::FromWebContents(restored_web_contents);
  ResetObserver();
  WaitUntilLanguageDetermined(restored_translate_client);
  EXPECT_EQ("fr",
            restored_translate_client->GetLanguageState().current_language());
}

// Test that hrefTranslate overrides manual translate
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       HrefTranslateOverridesManualTranslate) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->SetIgnoreMissingKeyForTesting(true);

  // Set target language manually
  manager->SetPredefinedTargetLanguage("ru");
  EXPECT_EQ("ru", chrome_translate_client->GetLanguageState()
                      .GetPredefinedTargetLanguage());

  SetTranslateScript(kTestValidScript);

  // Load a German page and detect it's language
  ASSERT_TRUE(AddTabAtIndex(0,
                            GURL(embedded_test_server()->GetURL(
                                "www.google.com", "/href_translate_test.html")),
                            ui::PAGE_TRANSITION_TYPED));
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Navigate to the French page by way of a link on the original page
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());

  // Href-translate to ja should override manual translate to ru.
  WaitUntilPageTranslated();
  EXPECT_EQ("ja",
            chrome_translate_client->GetLanguageState().current_language());
}

class TranslateManagerPrerenderBrowserTest
    : public TranslateManagerBrowserTest {
 public:
  TranslateManagerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &TranslateManagerPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(TranslateManagerPrerenderBrowserTest,
                       SkipPrerenderPage) {
  SetTranslateScript(kTestValidScript);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  base::HistogramTester histograms;

  // Load a French page.
  prerender_helper_.NavigatePrimaryPage(
      embedded_test_server()->GetURL("/french_page.html"));
  ResetObserver();

  // Prerender a German page.
  prerender_helper_.AddPrerender(
      embedded_test_server()->GetURL("/german_page.html"));

  // The prerendering page should not affect the primary page.
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);
  WaitUntilPageTranslated();
  EXPECT_FALSE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::NONE, GetPageTranslatedResult());
  histograms.ExpectTotalCount("Translate.LanguageDetection.ContentLength", 1);
  histograms.ExpectTotalCount("Translate.LanguageDeterminedDuration", 1);

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(
      embedded_test_server()->GetURL("/german_page.html"));

  // Check that the translation service still works well.
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  // TODO(crbug.com/40200965): Migrate to better mechanism for testing around
  // language detection.
  WaitUntilLanguageDetermined(chrome_translate_client);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().source_language());
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en",
      true);
  WaitUntilPageTranslated();
  EXPECT_FALSE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::NONE, GetPageTranslatedResult());
  histograms.ExpectTotalCount("Translate.LanguageDetection.ContentLength", 2);

  // Check noisy data was filtered out.
  histograms.ExpectTotalCount("Translate.LanguageDeterminedDuration", 1);
}

class TranslateManagerBackForwardCacheBrowserTest
    : public TranslateManagerBrowserTest {
 public:
  TranslateManagerBackForwardCacheBrowserTest() = default;
  ~TranslateManagerBackForwardCacheBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    TranslateManagerBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetupFeaturesAndParameters();
    TranslateManagerBrowserTest::SetUpCommandLine(command_line);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  GURL GetURL(const std::string& host) {
    return embedded_test_server()->GetURL(host, "/french_page.html");
  }

  void SetupFeaturesAndParameters() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting(
            {// Entry to the cache can be slow during testing and cause
             // flakiness.
             ::features::kBackForwardCacheEntryTimeout}));

    vmodule_switches_.InitWithSwitches("back_forward_cache_impl=1");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  logging::ScopedVmoduleSwitches vmodule_switches_;
};

IN_PROC_BROWSER_TEST_F(TranslateManagerBackForwardCacheBrowserTest,
                       RestorePageTranslatorAfterBackForwardCache) {
  SetTranslateScript(kTestValidScript);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("a.com")));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  WaitUntilLanguageDetermined(GetChromeTranslateClient());

  EXPECT_EQ("fr",
            GetChromeTranslateClient()->GetLanguageState().source_language());
  ResetObserver();
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());
  WaitUntilLanguageDetermined(GetChromeTranslateClient());

  // A is frozen in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  ResetObserver();

  // 3) Navigate back.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  WaitUntilLanguageDetermined(GetChromeTranslateClient());
  ResetObserver();

  // A is restored, B is stored.
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Translate the page through TranslateManager.
  TranslateManager* manager = GetChromeTranslateClient()->GetTranslateManager();

  LanguageState* language_state = manager->GetLanguageState();
  EXPECT_EQ(TranslationType::kUninitialized,
            language_state->translation_type());

  manager->TranslatePage(
      GetChromeTranslateClient()->GetLanguageState().source_language(), "en",
      true, TranslationType::kAutomaticTranslationByPref);

  EXPECT_EQ(TranslationType::kAutomaticTranslationByPref,
            language_state->translation_type());
  WaitUntilPageTranslated();
}

IN_PROC_BROWSER_TEST_F(TranslateManagerBackForwardCacheBrowserTest,
                       RestoreOriginStateAfterCache) {
  SetTranslateScript(kTestValidScript);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("a.com")));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined(chrome_translate_client);

  TranslateManager* manager = chrome_translate_client->GetTranslateManager();

  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().source_language(), "en", true,
      TranslationType::kAutomaticTranslationByPref);

  WaitUntilPageTranslated();

  {
    // Intentionally check translated state directly in blink. Without any
    // proxies.
    ASSERT_EQ(true, content::EvalJs(rfh_a.get(), "cr.googleTranslate.finished",
                                    content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                    ISOLATED_WORLD_ID_TRANSLATE));
    ASSERT_EQ(0, content::EvalJs(rfh_a.get(), "cr.googleTranslate.errorCode",
                                 content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                 ISOLATED_WORLD_ID_TRANSLATE));
  }

  ResetObserver();
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));

  // A is frozen in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  WaitUntilLanguageDetermined(GetChromeTranslateClient());

  // Navigate back.
  ResetObserver();
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  WaitUntilLanguageDetermined(GetChromeTranslateClient());

  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  {
    // Intentionally check restored state directly in blink. Without any
    // proxies.
    ASSERT_EQ(true, content::EvalJs(rfh_a.get(), "window.isTranslationRestored",
                                    content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                    ISOLATED_WORLD_ID_TRANSLATE));
    ASSERT_EQ(0, content::EvalJs(rfh_a.get(), "cr.googleTranslate.errorCode",
                                 content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                 ISOLATED_WORLD_ID_TRANSLATE));
  }
}

}  // namespace
}  // namespace translate
