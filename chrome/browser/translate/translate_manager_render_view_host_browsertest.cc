// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/buildflags.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/page_transition_types.h"
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
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          onTranslateProgress(100, true, false);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

class MockTranslateAgent : public translate::mojom::TranslateAgent {
 public:
  MockTranslateAgent() = default;
  ~MockTranslateAgent() override = default;

  mojo::PendingRemote<translate::mojom::TranslateAgent> BindToNewPageRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  // translate::mojom::TranslateAgent implementation.
  void TranslateFrame(const std::string& translate_script,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateFrameCallback callback) override {
    called_translate_ = true;
    source_lang_ = source_lang;
    target_lang_ = target_lang;
    std::move(callback).Run(true, source_lang, target_lang,
                            translate::TranslateErrors::NONE);
  }

  void RevertTranslation() override {}

#if BUILDFLAG(ENABLE_PDF)
  void PdfPageCaptured(const std::u16string& contents,
                       const std::string& pdf_lang,
                       const GURL& url) override {}
#endif

  bool called_translate_ = false;
  std::optional<std::string> source_lang_;
  std::optional<std::string> target_lang_;

 private:
  mojo::Receiver<translate::mojom::TranslateAgent> receiver_{this};
};

class MockTranslateBubbleFactory : public TranslateBubbleFactory {
 public:
  MockTranslateBubbleFactory() = default;

  MockTranslateBubbleFactory(const MockTranslateBubbleFactory&) = delete;
  MockTranslateBubbleFactory& operator=(const MockTranslateBubbleFactory&) =
      delete;

  ShowTranslateBubbleResult ShowImplementation(
      BrowserWindow* window,
      content::WebContents* web_contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type) override {
    if (model_) {
      model_->SetViewState(
          TranslateBubbleModelImpl::TranslateStepToViewState(step));
      return ShowTranslateBubbleResult::kSuccess;
    }

    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(web_contents);

    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
        new translate::TranslateUIDelegate(
            chrome_translate_client->GetTranslateManager()->GetWeakPtr(),
            source_language, target_language));
    model_ = std::make_unique<TranslateBubbleModelImpl>(step,
                                                        std::move(ui_delegate));
    return ShowTranslateBubbleResult::kSuccess;
  }

  bool DismissBubble() {
    if (!model_) {
      return false;
    }
    model_->DeclineTranslation();
    model_->OnBubbleClosing();
    model_.reset();
    return true;
  }

  TranslateBubbleModel* model() { return model_.get(); }

 private:
  std::unique_ptr<TranslateBubbleModel> model_;
};

class TranslateManagerRenderViewHostTest : public InProcessBrowserTest {
 public:
  TranslateManagerRenderViewHostTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    scoped_feature_list_.InitAndDisableFeature(toast_features::kTranslateToast);
  }

  TranslateManagerRenderViewHostTest(
      const TranslateManagerRenderViewHostTest&) = delete;
  TranslateManagerRenderViewHostTest& operator=(
      const TranslateManagerRenderViewHostTest&) = delete;

  ~TranslateManagerRenderViewHostTest() override = default;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SimulateNavigation(const GURL& url,
                          const std::string& lang,
                          bool page_translatable) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    SimulateOnTranslateLanguageDetermined(lang, page_translatable);
  }

  void SimulateOnTranslateLanguageDetermined(const std::string& lang,
                                             bool page_translatable) {
    translate::LanguageDetectionDetails details;
    details.adopted_language = lang;
    details.model_detected_language = lang;
    details.is_model_reliable = true;
    ChromeTranslateClient::FromWebContents(web_contents())
        ->translate_driver()
        ->RegisterPage(fake_agent_.BindToNewPageRemote(), details,
                       page_translatable);
  }

  void SimulateOnPageTranslated(const std::string& source_lang,
                                const std::string& target_lang,
                                translate::TranslateErrors error) {
    base::RunLoop().RunUntilIdle();
    auto* driver = static_cast<translate::ContentTranslateDriver*>(
        ChromeTranslateClient::FromWebContents(web_contents())
            ->translate_driver());
    driver->OnPageTranslated(/*cancelled=*/false, source_lang, target_lang,
                             error);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOnPageTranslated(const std::string& source_lang,
                                const std::string& target_lang) {
    SimulateOnPageTranslated(source_lang, target_lang,
                             translate::TranslateErrors::NONE);
  }

  bool GetTranslateMessage(std::string* source_lang, std::string* target_lang) {
    base::RunLoop().RunUntilIdle();
    if (!fake_agent_.called_translate_) {
      return false;
    }
    if (source_lang) {
      *source_lang = *fake_agent_.source_lang_;
    }
    if (target_lang) {
      *target_lang = *fake_agent_.target_lang_;
    }
    fake_agent_.called_translate_ = false;
    fake_agent_.source_lang_ = std::nullopt;
    fake_agent_.target_lang_ = std::nullopt;
    return true;
  }

  TestRenderViewContextMenu* CreateContextMenu() {
    content::ContextMenuParams params;
    params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
    params.x = 0;
    params.y = 0;
    params.has_image_contents = true;
    params.media_flags = 0;
    params.spellcheck_enabled = false;
    params.is_editable = false;
    params.page_url =
        web_contents()->GetController().GetLastCommittedEntry()->GetURL();
    params.edit_flags = blink::ContextMenuDataEditFlags::kCanTranslate;
    return new TestRenderViewContextMenu(*web_contents()->GetPrimaryMainFrame(),
                                         params);
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("www.google.fr", "127.0.0.1");
    host_resolver()->AddRule("www.google.mys", "127.0.0.1");

    translate::TranslateDownloadManager* download_manager =
        translate::TranslateDownloadManager::GetInstance();
    download_manager->ClearTranslateScriptForTesting();
    download_manager->SetTranslateScriptExpirationDelay(60 * 60 * 1000);
    download_manager->set_url_loader_factory(test_shared_loader_factory_);

    ChromeTranslateClient* client =
        ChromeTranslateClient::FromWebContents(web_contents());
    client->translate_driver()->set_translate_max_reload_attempts(0);
    client->GetTranslatePrefs()->ResetToDefaults();

    translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kTranslateScriptURL,
        "http://www.google.com/mock_translate_script.js");
    command_line->AppendSwitch(::switches::kOverrideLanguageDetection);
  }

  void TearDownOnMainThread() override {
    translate::TranslateManager::SetIgnoreMissingKeyForTesting(false);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SimulateTranslateScriptURLFetch(bool success) {
    GURL url = translate::TranslateDownloadManager::GetInstance()
                   ->script()
                   ->GetTranslateScriptURL();
    test_url_loader_factory_.AddResponse(
        url.spec(), success ? kTestValidScript : std::string(),
        success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR);
    base::RunLoop().RunUntilIdle();
    test_url_loader_factory_.ClearResponses();
  }

  void SimulateSupportedLanguagesURLFetch(
      bool success,
      const std::vector<std::string>& languages) {
    std::string data;
    if (success) {
      data = base::StringPrintf(
          "{\"sl\": {\"bla\": \"bla\"}, \"%s\": {",
          translate::TranslateLanguageList::kTargetLanguagesKey);
      const char* comma = "";
      for (size_t i = 0; i < languages.size(); ++i) {
        data += base::StringPrintf("%s\"%s\": \"UnusedFullName\"", comma,
                                   languages[i].c_str());
        if (i == 0) {
          comma = ",";
        }
      }
      data += "}}";
    }
    GURL url = translate::TranslateDownloadManager::GetInstance()
                   ->language_list()
                   ->LanguageFetchURLForTesting();
    test_url_loader_factory_.AddResponse(
        url.spec(), data,
        success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR);
    base::RunLoop().RunUntilIdle();
    test_url_loader_factory_.ClearResponses();
  }

  MockTranslateAgent fake_agent_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TranslateManagerRenderViewHostNoOverrideTest
    : public TranslateManagerRenderViewHostTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {}
};

// A list of languages to fake being returned by the translate server.
// Use only languages for which Chrome's copy of ICU has
// display names in English locale. To save space, Chrome's copy of ICU
// does not have the display name for a language unless it's in the
// Accept-Language list.
constexpr auto kServerLanguageList = std::to_array<const char*>({
    "ak",
    "af",
    "en-CA",
    "zh",
    "yi",
    "fr-FR",
    "tl",
    "iw",
    "hz",
    "xx",
});

// Test the fetching of languages from the translate server
IN_PROC_BROWSER_TEST_F(TranslateManagerRenderViewHostNoOverrideTest,
                       FetchLanguagesFromTranslateServer) {
  translate::TranslateDownloadManager::GetInstance()
      ->language_list()
      ->SetResourceRequestsAllowed(true);

  std::vector<std::string> server_languages(kServerLanguageList.begin(),
                                            kServerLanguageList.end());

  // First, get the default languages list. Note that calling
  // GetSupportedLanguages() invokes RequestLanguageList() internally.
  std::vector<std::string> default_supported_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      true /* translate_allowed */, &default_supported_languages);
  // To make sure we got the defaults and don't confuse them with the mocks.
  ASSERT_NE(default_supported_languages.size(), server_languages.size());

  // Check that we still get the defaults until the URLFetch has completed.
  std::vector<std::string> current_supported_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      true /* translate_allowed */, &current_supported_languages);
  EXPECT_EQ(default_supported_languages, current_supported_languages);

  // Also check that it didn't change if we failed the URL fetch.
  SimulateSupportedLanguagesURLFetch(false, std::vector<std::string>());
  current_supported_languages.clear();
  translate::TranslateDownloadManager::GetSupportedLanguages(
      true /* translate_allowed */, &current_supported_languages);
  EXPECT_EQ(default_supported_languages, current_supported_languages);

  // Now check that we got the appropriate set of languages from the server.
  SimulateSupportedLanguagesURLFetch(true, server_languages);
  current_supported_languages.clear();
  translate::TranslateDownloadManager::GetSupportedLanguages(
      true /* translate_allowed */, &current_supported_languages);
  // "in" is not in the kAcceptList and "xx" can't be displayed, so both are
  // removed from the downloaded list.
  EXPECT_EQ(server_languages.size() - 2, current_supported_languages.size());
  // Not sure we need to guarantee the order of languages, so we find them.
  for (size_t i = 0; i < server_languages.size(); ++i) {
    const std::string& lang = server_languages[i];
    if (lang == "xx" || lang == "hz") {
      continue;
    }
    EXPECT_TRUE(std::ranges::contains(current_supported_languages, lang))
        << "lang=" << lang;
  }
}

// The following tests depend on the translate bubble UI.
IN_PROC_BROWSER_TEST_F(TranslateManagerRenderViewHostTest,
                       BubbleNormalTranslate) {
  EXPECT_TRUE(TranslateService::IsTranslateBubbleEnabled());

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  std::unique_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Check the bubble exists.
  TranslateBubbleModel* bubble = factory->model();
  ASSERT_TRUE(bubble != nullptr);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble->GetViewState());

  // Simulate clicking translate.
  bubble->Translate();

  // Check the bubble shows "Translating...".
  bubble = factory->model();
  ASSERT_TRUE(bubble != nullptr);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            bubble->GetViewState());

  // Set up a simulation of the translate script being retrieved (it only
  // needs to be done once in the test as it is cached).
  SimulateTranslateScriptURLFetch(true);

  // Simulate the render notifying the translation has been done.
  SimulateOnPageTranslated("fr", "en");

  // Check the bubble shows "Translated."
  bubble = factory->model();
  ASSERT_TRUE(bubble != nullptr);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble->GetViewState());
}

IN_PROC_BROWSER_TEST_F(TranslateManagerRenderViewHostTest,
                       BubbleTranslateScriptNotAvailable) {
  EXPECT_TRUE(TranslateService::IsTranslateBubbleEnabled());

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  std::unique_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Check the bubble exists.
  TranslateBubbleModel* bubble = factory->model();
  ASSERT_TRUE(bubble != nullptr);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble->GetViewState());

  // Simulate clicking translate.
  bubble->Translate();
  SimulateTranslateScriptURLFetch(false);

  // We should not have sent any message to translate to the renderer.
  EXPECT_FALSE(GetTranslateMessage(nullptr, nullptr));

  // And we should have an error bubble showing.
  bubble = factory->model();
  ASSERT_TRUE(bubble != nullptr);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ERROR, bubble->GetViewState());
}

IN_PROC_BROWSER_TEST_F(TranslateManagerRenderViewHostNoOverrideTest,
                       BubbleUnknownLanguage) {
  EXPECT_TRUE(TranslateService::IsTranslateBubbleEnabled());

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  std::unique_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  // Simulate navigating to a page ("und" is the string returned by the CLD for
  // languages it does not recognize).
  SimulateNavigation(GURL("http://www.google.mys"), "und", true);

  // We should not have a bubble as we don't know the language.
  ASSERT_TRUE(factory->model() == nullptr);

  // Translate the page anyway through the context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);

  // Check the bubble exists.
  TranslateBubbleModel* bubble = factory->model();
  ASSERT_TRUE(bubble != nullptr);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            bubble->GetViewState());
}

}  // namespace
}  // namespace translate
