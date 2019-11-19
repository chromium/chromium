// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_accept_languages_factory.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/common/content_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace translate {

namespace {

static const char kTestValidScript[] =
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
    "        translatePage : function(originalLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          var error = (originalLang == 'auto') ? true : false;"
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
    "        translatePage : function(originalLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          onTranslateProgress(100, true, 0);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

static const char kTestScriptTimeout[] =
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
    "        translatePage : function(originalLang, targetLang,"
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
    "        translatePage : function(originalLang, targetLang,"
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
    "        translatePage : function(originalLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          var url = \"https://translate.googleapis.com/INVALID\";"
    "          cr.googleTranslate.onLoadJavascript(url);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

}  // namespace

class TranslateManagerBrowserTest : public InProcessBrowserTest {
 public:
  TranslateManagerBrowserTest() {
    error_subscription_ = TranslateManager::RegisterTranslateErrorCallback(
        base::Bind(&TranslateManagerBrowserTest::OnTranslateError,
                   base::Unretained(this)));
  }
  ~TranslateManagerBrowserTest() override {}

  void WaitUntilLanguageDetermined() { language_determined_waiter_->Wait(); }

  void WaitUntilPageTranslated() {
    TranslateWaiter(browser()->tab_strip_model()->GetActiveWebContents(),
                    TranslateWaiter::WaitEvent::kPageTranslated)
        .Wait();
  }

  void ResetObserver() {
    language_determined_waiter_ = std::make_unique<TranslateWaiter>(
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

  TranslateErrors::Type GetPageTranslatedResult() { return error_type_; }

  ChromeTranslateClient* GetChromeTranslateClient() {
    return ChromeTranslateClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 protected:
  // InProcessBrowserTest members.
  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    ResetObserver();
    error_type_ = TranslateErrors::NONE;

    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &TranslateManagerBrowserTest::HandleRequest, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Enable Experimental web platform features for HrefTranslate tests
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalWebPlatformFeatures);

    command_line->AppendSwitchASCII(
        switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
  }
  void TearDownOnMainThread() override {
    language_determined_waiter_.reset();

    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetTranslateScript(const std::string& script) { script_ = script; }

 private:
  TranslateErrors::Type error_type_;

  std::unique_ptr<TranslateManager::TranslateErrorCallbackList::Subscription>
      error_subscription_;

  std::unique_ptr<TranslateWaiter> language_determined_waiter_;

  std::string script_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManagerBrowserTest);
};

// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageLanguageDetection) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // The InProcessBrowserTest opens a new tab, let's wait for that first.
  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in English.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/english_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().original_language());

  ResetObserver();
  // Now navigate to a page in French.
  ui_test_utils::NavigateToURL(
      browser(), GURL(embedded_test_server()->GetURL("/french_page.html")));
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());
}

// Tests that the language detection / HTML attribute override works correctly.
// For languages in the whitelist, the detected language should override the
// HTML attribute. For all other languages, the HTML attribute should be used.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageLanguageDetectionConflict) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // The InProcessBrowserTest opens a new tab, let's wait for that first.
  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French with incorrect HTML language
  // attribute specified. The language attribute should be overridden by the
  // language detection.
  AddTabAtIndex(
      0,
      GURL(embedded_test_server()->GetURL("/french_page_lang_conflict.html")),
      ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in Korean with incorrect HTML language
  // attribute specified. The language attribute should not be overridden by the
  // language detection.
  AddTabAtIndex(
      0,
      GURL(embedded_test_server()->GetURL("/korean_page_lang_conflict.html")),
      ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().original_language());
}

// Test that the translation was successful.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationSuccess) {
  SetTranslateScript(kTestValidScript);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_FALSE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::NONE, GetPageTranslatedResult());
}

// Test that the translation was successful in an about:blank page.
// This is a regression test for https://crbug.com/943685.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationAboutBlank) {
  SetTranslateScript(kTestValidScript);
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
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

// Test that hrefTranslate is propagating properly
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateSuccess) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Load a German page and detect it's language
  AddTabAtIndex(0,
                GURL(embedded_test_server()->GetURL(
                    "www.google.com", "/href_translate_test.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();
  EXPECT_EQ("de",
            chrome_translate_client->GetLanguageState().original_language());

  // Navigate to the French page by way of a link on the original page
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // See that the page was translated automatically
  WaitUntilPageTranslated();
  EXPECT_EQ("ja",
            chrome_translate_client->GetLanguageState().current_language());

  // The target shouldn't be added to accept languages.
  EXPECT_FALSE(TranslateAcceptLanguagesFactory::GetForBrowserContext(
                   browser()->profile())
                   ->IsAcceptLanguage("ja"));
}

// Test that hrefTranslate doesn't auto-translate if the originator of the
// navigation isn't a Google origin.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       HrefTranslateNotFromGoogle) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Load a German page and detect it's language
  AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/href_translate_test.html")),
      ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();
  EXPECT_EQ("de",
            chrome_translate_client->GetLanguageState().original_language());

  // Navigate to the French page by way of a link on the original page
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  EXPECT_EQ("", chrome_translate_client->GetLanguageState().AutoTranslateTo());
}

// Test that hrefTranslate with an unsupported language doesn't trigger.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateUnsupported) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Load a German page and detect it's language
  AddTabAtIndex(0,
                GURL(embedded_test_server()->GetURL(
                    "www.google.com", "/href_translate_test.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();
  EXPECT_EQ("de",
            chrome_translate_client->GetLanguageState().original_language());

  // Navigate to the French page by way of a link on the original page. This
  // link has the hrefTranslate attribute set to "unsupported", so it shouldn't
  // trigger translate.
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { "
      "document.getElementById('test-unsupported-language').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  EXPECT_EQ("", chrome_translate_client->GetLanguageState().AutoTranslateTo());
}

// Test an href translate link to a conflicted page
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateConflict) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Load a German page and detect it's language
  AddTabAtIndex(0,
                GURL(embedded_test_server()->GetURL(
                    "www.google.com", "/href_translate_test.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();
  EXPECT_EQ("de",
            chrome_translate_client->GetLanguageState().original_language());

  // Navigate to the French page that thinks its in English by way of a link on
  // the original page
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test-conflict').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // See that the page was translated automatically
  WaitUntilPageTranslated();
  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().current_language());
}

// Test an href translate link without an href lang for the landing page
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, HrefTranslateNoHrefLang) {
  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();
  chrome_translate_client->GetTranslateManager()->SetIgnoreMissingKeyForTesting(
      true);
  SetTranslateScript(kTestValidScript);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Load a German page and detect it's language
  AddTabAtIndex(0,
                GURL(embedded_test_server()->GetURL(
                    "www.google.com", "/href_translate_test.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();
  EXPECT_EQ("de",
            chrome_translate_client->GetLanguageState().original_language());

  // Use a link with no hrefLang to navigate to a French page
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test-no-hrefLang').click(); "
      "})();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // See that the page was translated automatically
  WaitUntilPageTranslated();
  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().current_language());
}

// Test if there was an error during translation.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationError) {
  SetTranslateScript(kTestValidScript);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with about:blank page.
  AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
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

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
      true);

  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
  EXPECT_EQ(TranslateErrors::INITIALIZATION_ERROR, GetPageTranslatedResult());
}

// Test the checks translate lib never gets ready and throws timeout.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PageTranslationTimeoutError) {
  SetTranslateScript(kTestScriptTimeout);

  ChromeTranslateClient* chrome_translate_client = GetChromeTranslateClient();

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
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

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

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

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
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

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
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

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  TranslateManager* manager = chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
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

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  ResetObserver();

  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ui_test_utils::NavigateToURL(browser(), french_url);

  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());
}

IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       TranslateSessionRestore) {
  ChromeTranslateClient* active_translate_client = GetChromeTranslateClient();
  if (active_translate_client->GetLanguageState().current_language().empty())
    WaitUntilLanguageDetermined();
  EXPECT_EQ("und",
            active_translate_client->GetLanguageState().current_language());

  // Make restored tab active to (on some platforms) initiate language
  // detection.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});

  content::WebContents* restored_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeTranslateClient* restored_translate_client =
      ChromeTranslateClient::FromWebContents(restored_web_contents);
  if (restored_translate_client->GetLanguageState()
          .current_language()
          .empty()) {
    ResetObserver();
    WaitUntilLanguageDetermined();
  }
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
  EXPECT_EQ("ru", manager->GetLanguageState().GetPredefinedTargetLanguage());

  SetTranslateScript(kTestValidScript);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetermined();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Load a German page and detect it's language
  AddTabAtIndex(0,
                GURL(embedded_test_server()->GetURL(
                    "www.google.com", "/href_translate_test.html")),
                ui::PAGE_TRANSITION_TYPED);
  ResetObserver();
  chrome_translate_client = GetChromeTranslateClient();
  WaitUntilLanguageDetermined();
  EXPECT_EQ("de",
            chrome_translate_client->GetLanguageState().original_language());

  // Navigate to the French page by way of a link on the original page
  ResetObserver();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  const std::string click_link_js =
      "(function() { document.getElementById('test').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_link_js));

  // Detect language on the new page
  WaitUntilLanguageDetermined();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Href-translate to ja should override manual translate to ru.
  WaitUntilPageTranslated();
  EXPECT_EQ("ja",
            chrome_translate_client->GetLanguageState().current_language());
}

}  // namespace translate
