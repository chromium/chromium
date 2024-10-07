// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/fake_translate_agent.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace translate {
namespace {

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
      return ShowTranslateBubbleResult::SUCCESS;
    }

    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(web_contents);

    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
        new translate::TranslateUIDelegate(
            chrome_translate_client->GetTranslateManager()->GetWeakPtr(),
            source_language, target_language));
    model_ = std::make_unique<TranslateBubbleModelImpl>(step,
                                                        std::move(ui_delegate));
    return ShowTranslateBubbleResult::SUCCESS;
  }

  bool DismissBubble() {
    if (!model_)
      return false;
    model_->DeclineTranslation();
    model_->OnBubbleClosing();
    model_.reset();
    return true;
  }

  TranslateBubbleModel* model() { return model_.get(); }

 private:
  std::unique_ptr<TranslateBubbleModel> model_;
};

class TranslateManagerRenderViewHostTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TranslateManagerRenderViewHostTest()
      : pref_callback_(base::BindRepeating(
            &TranslateManagerRenderViewHostTest::OnPreferenceChanged,
            base::Unretained(this))),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  TranslateManagerRenderViewHostTest(
      const TranslateManagerRenderViewHostTest&) = delete;
  TranslateManagerRenderViewHostTest& operator=(
      const TranslateManagerRenderViewHostTest&) = delete;

  // Simulates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url,
                          const std::string& lang,
                          bool page_translatable) {
    if (main_rfh()->GetLastCommittedURL() == url) {
      content::NavigationSimulator::Reload(web_contents());
    } else {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                                 url);
    }
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

  void SimulateOnPageTranslated(const std::string& source_lang,
                                const std::string& target_lang,
                                translate::TranslateErrors error) {
    // Ensure fake_agent_ Translate() call gets dispatched.
    base::RunLoop().RunUntilIdle();

    fake_agent_.PageTranslated(false, source_lang, target_lang, error);

    // Ensure fake_agent_ Translate() response callback gets dispatched.
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOnPageTranslated(const std::string& source_lang,
                                const std::string& target_lang) {
    SimulateOnPageTranslated(source_lang, target_lang,
                             translate::TranslateErrors::NONE);
  }

  bool GetTranslateMessage(std::string* source_lang, std::string* target_lang) {
    base::RunLoop().RunUntilIdle();

    if (!fake_agent_.called_translate_)
      return false;
    EXPECT_TRUE(fake_agent_.source_lang_);
    EXPECT_TRUE(fake_agent_.target_lang_);

    if (source_lang)
      *source_lang = *fake_agent_.source_lang_;
    if (target_lang)
      *target_lang = *fake_agent_.target_lang_;

    // Reset
    fake_agent_.called_translate_ = false;
    fake_agent_.source_lang_ = std::nullopt;
    fake_agent_.target_lang_ = std::nullopt;

    return true;
  }

  bool IsTranslationReverted() {
    base::RunLoop().RunUntilIdle();
    return fake_agent_.called_revert_translation_;
  }

  void ReloadAndWait(bool successful_reload) {
    if (successful_reload) {
      content::NavigationSimulator::Reload(web_contents());
    } else {
      content::NavigationSimulator::ReloadAndFail(web_contents(),
                                                  net::ERR_TIMED_OUT);
    }
    // The TranslateManager class processes the navigation entry committed
    // notification in a posted task; process that task.
    base::RunLoop().RunUntilIdle();
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
#if BUILDFLAG(IS_MAC)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // BUILDFLAG(IS_MAC)
    params.edit_flags = blink::ContextMenuDataEditFlags::kCanTranslate;
    return new TestRenderViewContextMenu(*web_contents()->GetPrimaryMainFrame(),
                                         params);
  }

  MOCK_METHOD1(OnPreferenceChanged, void(const std::string&));

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
    translate::TranslateDownloadManager* download_manager =
        translate::TranslateDownloadManager::GetInstance();
    download_manager->ClearTranslateScriptForTesting();
    download_manager->SetTranslateScriptExpirationDelay(60 * 60 * 1000);
    download_manager->set_url_loader_factory(test_shared_loader_factory_);

    ChromeTranslateClient::CreateForWebContents(web_contents());
    ChromeTranslateClient::FromWebContents(web_contents())
        ->translate_driver()
        ->set_translate_max_reload_attempts(0);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    TranslateService::ShutdownForTesting();
  }

  void SimulateTranslateScriptURLFetch(bool success) {
    GURL url = translate::TranslateDownloadManager::GetInstance()
                   ->script()
                   ->GetTranslateScriptURL();
    test_url_loader_factory_.AddResponse(
        url.spec(), std::string(),
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
        data += base::StringPrintf(
            "%s\"%s\": \"UnusedFullName\"", comma, languages[i].c_str());
        if (i == 0)
          comma = ",";
      }
      data += "}}";
    }
    GURL url = translate::TranslateDownloadManager::GetInstance()
                   ->language_list()
                   ->LanguageFetchURLForTesting();
    EXPECT_TRUE(test_url_loader_factory_.IsPending(url.spec()));
    test_url_loader_factory_.AddResponse(
        url.spec(), data,
        success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR);
    EXPECT_FALSE(test_url_loader_factory_.IsPending(url.spec()));
    base::RunLoop().RunUntilIdle();

    test_url_loader_factory_.ClearResponses();
  }

  void SetPrefObserverExpectation(const char* path) {
    EXPECT_CALL(*this, OnPreferenceChanged(std::string(path)));
  }

  PrefChangeRegistrar::NamedChangeCallback pref_callback_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  std::unique_ptr<MockTranslateBubbleFactory> bubble_factory_;
  FakeTranslateAgent fake_agent_;
};

// A variant of the above test class that sets the UI language to an invalid
// code (and restores it afterwards). This is required because a significant
// amount of logic using the UI language has already occured by the time we
// enter an individual test body.

static const char* kInvalidLocale = "qbz";
class TranslateManagerRenderViewHostInvalidLocaleTest
    : public TranslateManagerRenderViewHostTest {
 public:
  TranslateManagerRenderViewHostInvalidLocaleTest()
      : original_locale_(g_browser_process->GetApplicationLocale()) {
    SetApplicationLocale(kInvalidLocale);
  }

  TranslateManagerRenderViewHostInvalidLocaleTest(
      const TranslateManagerRenderViewHostInvalidLocaleTest&) = delete;
  TranslateManagerRenderViewHostInvalidLocaleTest& operator=(
      const TranslateManagerRenderViewHostInvalidLocaleTest&) = delete;

  ~TranslateManagerRenderViewHostInvalidLocaleTest() override {
    SetApplicationLocale(original_locale_);
  }

 private:
  const std::string original_locale_;

  void SetApplicationLocale(const std::string& locale) {
    g_browser_process->SetApplicationLocale(locale);
    translate::TranslateDownloadManager::GetInstance()->set_application_locale(
        g_browser_process->GetApplicationLocale());
  }
};

// A list of languages to fake being returned by the translate server.
// Use only languages for which Chrome's copy of ICU has
// display names in English locale. To save space, Chrome's copy of ICU
// does not have the display name for a language unless it's in the
// Accept-Language list.
static const char* kServerLanguageList[] = {"ak",    "af", "en-CA", "zh", "yi",
                                            "fr-FR", "tl", "iw",    "hz", "xx"};

// Test the fetching of languages from the translate server
TEST_F(TranslateManagerRenderViewHostTest, FetchLanguagesFromTranslateServer) {
  std::vector<std::string> server_languages;
  for (size_t i = 0; i < std::size(kServerLanguageList); ++i)
    server_languages.push_back(kServerLanguageList[i]);

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
    if (lang == "xx") {
      continue;
    } else if (lang == "hz") {
      continue;
    }
    EXPECT_TRUE(base::Contains(current_supported_languages, lang))
        << "lang=" << lang;
  }
}

// The following tests depend on the translate bubble UI.
TEST_F(TranslateManagerRenderViewHostTest, BubbleNormalTranslate) {
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

TEST_F(TranslateManagerRenderViewHostTest, BubbleTranslateScriptNotAvailable) {
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

TEST_F(TranslateManagerRenderViewHostTest, BubbleUnknownLanguage) {
  EXPECT_TRUE(TranslateService::IsTranslateBubbleEnabled());

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  std::unique_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  // Simulate navigating to a page ("und" is the string returned by the CLD for
  // languages it does not recognize).
  SimulateNavigation(GURL("http://www.google.mys"), "und", true);

  // We should not have a bubble as we don't know the language.
  ASSERT_TRUE(factory->model() == nullptr);

  // Translate the page anyway throught the context menu.
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
