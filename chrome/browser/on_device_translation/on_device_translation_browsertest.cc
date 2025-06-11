// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/service_controller_manager.h"
#include "chrome/browser/on_device_translation/test/test_util.h"
#include "chrome/browser/on_device_translation/translation_manager_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/test/test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"

using ::blink::mojom::CanCreateTranslatorResult;
using ::blink::mojom::TranslatorLanguageCode;
using ::content::JsReplace;
using ::testing::_;
using ::testing::Invoke;

namespace on_device_translation {

namespace {

constexpr auto kLanguagePackKeys = base::MakeFixedFlatSet<LanguagePackKey>({
    LanguagePackKey::kEn_Es, LanguagePackKey::kEn_Ja,
    LanguagePackKey::kAr_En, LanguagePackKey::kBn_En,
    LanguagePackKey::kDe_En, LanguagePackKey::kEn_Fr,
    LanguagePackKey::kEn_Hi, LanguagePackKey::kEn_It,
    LanguagePackKey::kEn_Ko, LanguagePackKey::kEn_Nl,
    LanguagePackKey::kEn_Pl, LanguagePackKey::kEn_Pt,
    LanguagePackKey::kEn_Ru, LanguagePackKey::kEn_Th,
    LanguagePackKey::kEn_Tr, LanguagePackKey::kEn_Vi,
    LanguagePackKey::kEn_Zh, LanguagePackKey::kEn_ZhHant,
    LanguagePackKey::kBg_En, LanguagePackKey::kCs_En,
    LanguagePackKey::kDa_En, LanguagePackKey::kEl_En,
    LanguagePackKey::kEn_Fi, LanguagePackKey::kEn_Hr,
    LanguagePackKey::kEn_Hu, LanguagePackKey::kEn_Id,
    LanguagePackKey::kEn_Iw, LanguagePackKey::kEn_Lt,
    LanguagePackKey::kEn_No, LanguagePackKey::kEn_Ro,
    LanguagePackKey::kEn_Sk, LanguagePackKey::kEn_Sl,
    LanguagePackKey::kEn_Sv, LanguagePackKey::kEn_Uk,
    LanguagePackKey::kEn_Kn, LanguagePackKey::kEn_Ta,
    LanguagePackKey::kEn_Te, LanguagePackKey::kEn_Mr,
});
static_assert(std::size(kLanguagePackKeys) ==
              static_cast<size_t>(LanguagePackKey::kMaxValue) + 1);

std::string GetPreferredLanguageString(
    const base::span<const LanguagePackKey>& language_pack_keys) {
  // Get unique set of language codes from the keys.
  std::set<std::string_view> language_codes;
  for (const auto& language_pack_key : language_pack_keys) {
    language_codes.insert(GetSourceLanguageCode(language_pack_key));
    language_codes.insert(GetTargetLanguageCode(language_pack_key));
  }

  // Create a preferred string
  std::string selected_languages = "";
  for (auto language_code : language_codes) {
    selected_languages += language_code;
    selected_languages += ",";
  }

  // Remove the extra comma at the end.
  selected_languages.pop_back();

  return selected_languages;
}

// Handles HTTP requests to `path` with `content` as the response body.
// `content` is expected to be JavaScript; the response mime type is always
// set to "text/javascript". Invokes `done_callback` after serving the HTTP
// request.
std::unique_ptr<net::test_server::HttpResponse> RespondWithJS(
    const std::string& path,
    const std::string& content,
    base::OnceClosure done_callback,
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  if (request_url.path() != path) {
    return nullptr;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/javascript");
  response->set_content(content);
  std::move(done_callback).Run();
  return response;
}

// Sets the path of the mock library to the command line.
void SetMockLibraryPathToCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchPath(kTranslateKitBinaryPath, GetMockLibraryPath());
}

// Writes fake dictionary data to a file and sets the path of the file to the
// command line.
void WriteFakeDictionaryDataAndSetCommandLine(LanguagePackKey key,
                                              const base::FilePath& temp_dir,
                                              base::CommandLine* command_line) {
  const auto dict_dir_path =
      temp_dir.AppendASCII(GetPackageInstallDirName(key));
  const auto dict_path = dict_dir_path.AppendASCII("dict.dat");
  CHECK(base::CreateDirectory(dict_dir_path));
  CHECK(base::File(dict_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
            .WriteAndCheck(0, base::as_byte_span(CreateFakeDictionaryData(
                                  GetSourceLanguageCode(key),
                                  GetTargetLanguageCode(key)))));
  command_line->AppendSwitchASCII(
      "translate-kit-packages",
      base::StrCat({GetSourceLanguageCode(key), ",", GetTargetLanguageCode(key),
                    ",", dict_dir_path.AsUTF8Unsafe()}));
}

// Returns a string representation of the result of TranslationAvailable().
std::string_view GetCanCreateTranslatorResultString(
    CanCreateTranslatorResult result) {
  switch (result) {
    case CanCreateTranslatorResult::kReadily:
      return "available";
    case CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady:
    case CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
    case CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired:
      return "downloadable";
    case CanCreateTranslatorResult::kNoNotSupportedLanguage:
    case CanCreateTranslatorResult::kNoServiceCrashed:
    case CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case CanCreateTranslatorResult::kNoExceedsServiceCountLimitation:
      return "unavailable";
  }
}

void Sleep(base::TimeDelta delay) {
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), delay);
  loop.Run();
}

// An implementation of SupportsUserData to be used in tests.
class TestSupportsUserData : public base::SupportsUserData {
 public:
  TestSupportsUserData() = default;
  ~TestSupportsUserData() override = default;
};

}  // namespace

class OnDeviceTranslationBrowserTest : public InProcessBrowserTest {
 public:
  OnDeviceTranslationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kTranslationAPI);
    CHECK(tmp_dir_.CreateUniqueTempDir());
  }
  ~OnDeviceTranslationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_https_test_server().ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 protected:
  const base::FilePath& GetTempDir() { return tmp_dir_.GetPath(); }

  content::BrowserContext* GetBrowserContext() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetBrowserContext();
  }

  const url::Origin GetLastCommittedOrigin() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame()
        ->GetLastCommittedOrigin();
  }

  // Navigates to an empty page.
  void NavigateToEmptyPage() {
    CHECK(ui_test_utils::NavigateToURL(
        browser(), embedded_https_test_server().GetURL("/empty.html")));
  }

  // Sets the SelectedLanguages prefs to the given value. This will change the
  // AcceptLanguages pref.
  void SetSelectedLanguages(const std::string_view value) {
    browser()->profile()->GetPrefs()->SetString(
        language::prefs::kSelectedLanguages, value);
  }

  // Sets the SelectedLanguages prefs to support all the languages in the
  // `language_pack_keys`. This will change the AcceptLanguages pref.
  void SetSelectedLanguages(
      const base::span<const LanguagePackKey>& language_pack_keys) {
    SetSelectedLanguages(GetPreferredLanguageString(language_pack_keys));
  }

  // Tests the behavior of availability().
  void TestCanTranslateResult(const std::string_view sourceLang,
                              const std::string_view targetLang,
                              CanCreateTranslatorResult expected_result) {
    NavigateToEmptyPage();
    // Call TranslationAvailable() via mojo interface to verify the detailed
    // result.
    mojo::Remote<blink::mojom::TranslationManager> remote;
    TestSupportsUserData fake_user_data;
    TranslationManagerImpl::Bind(GetBrowserContext(), &fake_user_data,
                                 GetLastCommittedOrigin(),
                                 remote.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    remote->TranslationAvailable(
        TranslatorLanguageCode::New(std::string(sourceLang)),
        TranslatorLanguageCode::New(std::string(targetLang)),
        base::BindLambdaForTesting([&](CanCreateTranslatorResult result) {
          EXPECT_EQ(result, expected_result);
          run_loop.Quit();
        }));
    run_loop.Run();

    // Need to navigate to an empty page to reset the state of the
    // TranslationManagerImpl.
    NavigateToEmptyPage();
    // Calls TranslationAvailable() via JS API (Translator.availability()) to
    // verify the result string.
    TestTranslationAvailable(
        browser(), sourceLang, targetLang,
        GetCanCreateTranslatorResultString(expected_result));
  }

  content::EvalJsResult EvalJs(
      std::string_view script,
      Browser* target_browser = nullptr,
      int options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return content::EvalJs((target_browser ? target_browser : browser())
                               ->tab_strip_model()
                               ->GetActiveWebContents(),
                           script, options);
  }

  testing::AssertionResult ExecJs(
      std::string_view script,
      Browser* target_browser = nullptr,
      int options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return content::ExecJs((target_browser ? target_browser : browser())
                               ->tab_strip_model()
                               ->GetActiveWebContents(),
                           script, options);
  }

  // Evaluates the given script and returns the result string. If the script
  // throws an error, returns the error message.
  // When `target_browser` is not nullptr, the script is evaluated in the
  // context of the given browser, otherwise the script is evaluated in the
  // context of the default browser.
  std::string EvalJsCatchingError(
      std::string_view script,
      Browser* target_browser = nullptr,
      int options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return EvalJs(base::StringPrintf(R"(
      (async () => {
        try {
          %s
        } catch (e) {
          return e.toString();
        }
      })();
    )",
                                     script),
                  target_browser, options)
        .ExtractString();
  }

  // Creates a console observer for the given pattern. When `target_browser`
  // is not nullptr, the observer is created in the context of the given
  // browser, otherwise the observer is created in the context of the default
  // browser.
  std::unique_ptr<content::WebContentsConsoleObserver> CreateConsoleObserver(
      const std::string_view pattern,
      Browser* target_browser = nullptr) {
    auto observer = std::make_unique<content::WebContentsConsoleObserver>(
        (target_browser ? target_browser : browser())
            ->tab_strip_model()
            ->GetActiveWebContents());
    observer->SetPattern(std::string(pattern));
    return observer;
  }

  // Waits for the console observer to receive a message.
  void WaitForConsoleObserver(
      content::WebContentsConsoleObserver& console_observer) {
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_FALSE(console_observer.messages().empty());
  }

  void ClearSiteContentSettings() {
    content::BrowsingDataRemover* remover =
        browser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(),
        chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS,
        chrome_browsing_data_remover::ALL_ORIGIN_TYPES, &observer);
    observer.BlockUntilCompletion();
  }

  content::RenderFrameHost* CreateIframe(Browser* target_browser = nullptr) {
    EXPECT_EQ(EvalJsCatchingError(R"(
      window._iframe = document.createElement('iframe');
      document.body.appendChild(window._iframe);
      return "OK";
  )",
                                  target_browser),
              "OK");

    return ChildFrameAt((target_browser ? target_browser : browser())
                            ->tab_strip_model()
                            ->GetActiveWebContents(),
                        0);
  }

  bool RemoveIframe(Browser* target_browser = nullptr) {
    return ExecJs("document.body.removeChild(window._iframe);");
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the behavior of create() when the library is installed before
// the language pack.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorInstallLibraryAndThenLanguagePack) {
  MockComponentManager mock_component_manager(GetTempDir());
  NavigateToEmptyPage();

  base::RunLoop run_loop_for_register_translate_kit;
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .WillOnce(Invoke([&]() { run_loop_for_register_translate_kit.Quit(); }));

  base::RunLoop run_loop_for_register_language_pack;
  EXPECT_CALL(mock_component_manager,
              RegisterTranslateKitLanguagePackComponent(_))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Ja);
        run_loop_for_register_language_pack.Quit();
      }));

  // Create a translator.
  EXPECT_EQ(EvalJsCatchingError(R"(
      window._testPromise = Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
      window._testPromiseResolved = false;
      window._testPromise.then(() => {
        window._testPromiseResolved = true;
      });
      return 'OK';
  )"),
            "OK");

  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called.
  run_loop_for_register_language_pack.Run();
  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();

  // The promise of create() should not be resolved yet.
  EXPECT_FALSE(EvalJs("window._testPromiseResolved").ExtractBool());

  // Install the mock language pack.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // Translate "hello" to Japanese.
  // Note: the mock TranslateKit component returns the concatenation of the
  // content of "dict.dat" in the language pack and the input text.
  // See comments in mock_translate_kit_lib.cc for more details.
  EXPECT_EQ(EvalJsCatchingError(
                "return await (await window._testPromise).translate('hello');"),
            "en to ja: hello");
}

// Tests the behavior of create() when the language pack is installed
// before the library.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorInstallLanguagePackAndThenLibrary) {
  MockComponentManager mock_component_manager(GetTempDir());
  NavigateToEmptyPage();

  base::RunLoop run_loop_for_register_translate_kit;
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .WillOnce(Invoke([&]() { run_loop_for_register_translate_kit.Quit(); }));

  base::RunLoop run_loop_for_register_language_pack;
  EXPECT_CALL(mock_component_manager,
              RegisterTranslateKitLanguagePackComponent(_))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Ja);
        run_loop_for_register_language_pack.Quit();
      }));

  // Create a translator.
  EXPECT_EQ(EvalJsCatchingError(R"(
      window._testPromise = Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
      window._testPromiseResolved = false;
      window._testPromise.then(() => {
        window._testPromiseResolved = true;
      });
      return 'OK';
  )"),
            "OK");

  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called.
  run_loop_for_register_language_pack.Run();

  // Install the mock language pack.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // The promise of create() should not be resolved yet.
  EXPECT_FALSE(EvalJs("window._testPromiseResolved").ExtractBool());

  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();
  // Translate "hello" to Japanese.
  EXPECT_EQ(EvalJsCatchingError(
                "return await (await window._testPromise).translate('hello');"),
            "en to ja: hello");
}

// TODO(crbug.com/421947718): Disabled because there's a race between triggering
// user activation and consuming it when calling `create` multiple times.
//
// Tests the behavior of multiple create() calls with different
// source/target languages.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       DISABLED_CreateTranslatorInstallMultipleLanguagePacks) {
  MockComponentManager mock_component_manager(GetTempDir());
  NavigateToEmptyPage();

  base::RunLoop run_loop_for_register_translate_kit;
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .WillOnce(Invoke([&]() { run_loop_for_register_translate_kit.Quit(); }));

  base::RunLoop run_loop_for_register_en_ja_language_pack;
  base::RunLoop run_loop_for_register_en_es_language_pack;
  EXPECT_CALL(mock_component_manager,
              RegisterTranslateKitLanguagePackComponent(_))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Ja);
        // Call RegisterLanguagePack() to avoid redundant calls of
        // RegisterTranslateKitLanguagePackComponent().
        mock_component_manager.RegisterLanguagePack(key);
        run_loop_for_register_en_ja_language_pack.Quit();
      }))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Es);
        mock_component_manager.RegisterLanguagePack(key);
        run_loop_for_register_en_es_language_pack.Quit();
      }));

  // Helper function to get the state of a promise at the moment the helper
  // function is called.
  EXPECT_TRUE(ExecJs(R"(
    self.getPromiseState = async promise => {
        const symbol = Symbol();
        try {
          const result = await Promise.race([promise, Promise.resolve(symbol)]);
          return result == symbol ? "pending" : "fulfilled";
        } catch (e) {
          return "rejected";
        }
    }
  )"));

  // Create create() multiple times.
  //   1. En => Ja.
  //   2. En => Es.
  //   3. En => Ja.
  EXPECT_TRUE(ExecJs(R"(
      self.enJaPromise1 = Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
  )",
                     browser(), content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  EXPECT_TRUE(ExecJs(R"(
      self.enEsPromise = Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'es',
        });
  )",
                     browser(), content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  EXPECT_TRUE(ExecJs(R"(
      self.enJaPromise2 = Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
  )",
                     browser(), content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called for
  // `en_ja` and `en_es`.
  run_loop_for_register_en_ja_language_pack.Run();
  run_loop_for_register_en_es_language_pack.Run();

  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();

  // All promises should not be resolved yet.
  EXPECT_EQ(EvalJs("getPromiseState(enJaPromise1)").ExtractString(), "pending");
  EXPECT_EQ(EvalJs("getPromiseState(enEsPromise)").ExtractString(), "pending");
  EXPECT_EQ(EvalJs("getPromiseState(enJaPromise2)").ExtractString(), "pending");

  // Install the mock `en_ja` language pack.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // Translate to Japanese. Both `en_ja` promises should be resolved now.
  EXPECT_EQ(EvalJsCatchingError(
                "return await (await enJaPromise1).translate('hello');"),
            "en to ja: hello");
  EXPECT_EQ(
      EvalJsCatchingError("return await (await enJaPromise2).translate('hi');"),
      "en to ja: hi");

  // The promise of `en_es` should not be resolved yet.
  EXPECT_EQ(EvalJs("getPromiseState(enEsPromise)").ExtractString(), "pending");

  // Install the mock `en_es` language pack.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Es);

  // Translate to Spanish. The `en_es` promise should be resolved now.
  EXPECT_EQ(EvalJsCatchingError(
                "return await (await self.enEsPromise).translate('hello');"),
            "en to es: hello");
}

// TODO(crbug.com/421947718): Disabled because there's a race between triggering
// user activation and consuming it when calling `create` multiple times.
//
// Tests the behavior of create() when the number of pending tasks
// exceeds the limit.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       DISABLED_ExceedMaxPendingTaskCount) {
  MockComponentManager mock_component_manager(GetTempDir());
  NavigateToEmptyPage();

  base::RunLoop run_loop_for_register_translate_kit;
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .WillOnce(Invoke([&]() { run_loop_for_register_translate_kit.Quit(); }));

  base::RunLoop run_loop_for_register_language_pack;
  EXPECT_CALL(mock_component_manager,
              RegisterTranslateKitLanguagePackComponent(_))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Ja);
        // Call RegisterLanguagePack() to avoid redundant calls of
        // RegisterTranslateKitLanguagePackComponent().
        mock_component_manager.RegisterLanguagePack(key);
        run_loop_for_register_language_pack.Quit();
      }));

  // TODO(crbug.com/421947718): Each `Translator.create` call should be in it's
  // own `EvalJs` call like
  // `CreateTranslator_Delay_ForTranslatorCreatedDuringInitialTranslatorCreationWithDelay`,
  // but since we're blocked on the race issue from crbug.com/421947718, this
  // hasn't been updated yet.
  //
  // Call create() kMaxPendingTaskCount times.
  EXPECT_EQ(EvalJsCatchingError(base::StringPrintf(R"(
      window._testPromises = [];
      window._testPromisesResolved = false;
      const kMaxPendingTaskCount = %zd;
      for (let i = 0; i < kMaxPendingTaskCount; ++i) {
        const promise = Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
        promise.then(() => {
          window._testPromisesResolved = true;
        });
        window._testPromises.push(promise);
      }
      return 'OK';
  )",
                                                   kMaxPendingTaskCount)),
            "OK");
  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called.
  run_loop_for_register_language_pack.Run();
  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();

  // Any promise should not be resolved yet.
  EXPECT_FALSE(EvalJs("window._testPromisesResolved").ExtractBool());

  auto console_observer =
      CreateConsoleObserver("Too many Translator API requests are queued.");

  // Calling create() one more time fails.
  EXPECT_EQ(EvalJsCatchingError(R"(
      await Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
      return 'OK';
  )"),
            "NotSupportedError: Unable to create translator for the given "
            "source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);

  // The all 1024 promises should not be resolved yet.
  EXPECT_FALSE(EvalJs("window._testPromisesResolved").ExtractBool());

  // Install the mock language pack.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // All promises should be resolved now. The all 1024 translators should
  // be able to translate.
  EXPECT_EQ(EvalJsCatchingError(R"(
      const translators = await Promise.all(window._testPromises);
      const results = await Promise.all(translators.map((translator) => {
        return translator.translate('hello');
      }));
      for (const result of results) {
        if (result != 'en to ja: hello') {
          return `Unexpected result ${result}`;
        }
      }
      return 'OK';
  )"),
            "OK");
}

// Tests the behavior of the failure of translation.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest, TranslationFailure) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToEmptyPage();

  // Tries to translate "SIMULATE_ERROR" to Japanese. This "SIMULATE_ERROR"
  // causes failure in the mock TranslateKit component. See comments in
  // mock_translate_kit_lib.cc.

  EXPECT_EQ(EvalJsCatchingError(R"(
      const translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'ja',
      });
      return await translator.translate('SIMULATE_ERROR');
  )"),
            "UnknownError: Other generic failures occurred.");
}

// Tests the behavior of the crash of calling translate().
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest, CrashWhileTranslating) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToEmptyPage();

  // Tries to translate "CAUSE_CRASH" to Japanese. This "CAUSE_CRASH" causes
  // a crash in the mock TranslateKit component. See comments in
  // mock_translate_kit_lib.cc.
  EXPECT_EQ(EvalJsCatchingError(R"(
      window._translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'ja',
      });
      return await window._translator.translate('CAUSE_CRASH');
  )"),
            "UnknownError: Other generic failures occurred.");

  // After the crash, the translator is not usable.
  EXPECT_EQ(EvalJsCatchingError(
                "return await window._translator.translate('hello');"),
            "UnknownError: Other generic failures occurred.");

  // But a new translator can be created and used.
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// Tests the behavior of failing to load the library
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorErrorLibraryLoadFailed) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(1);
  mock_component_manager.InstallEmptyMockComponent();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();

  auto console_observer =
#if BUILDFLAG(IS_WIN)
      // On Windows, the service crashes when failed to preload the library
      // in PreLockdownSandboxHook().
      CreateConsoleObserver("The translation service crashed.");
#else
      CreateConsoleObserver("Failed to load the translation library.");
#endif  // BUILDFLAG(IS_WIN)

  EXPECT_EQ(EvalJsCatchingError(R"(
            const translator = await Translator.create({
              sourceLanguage: 'en',
              targetLanguage: 'ja',
            });
      )"),
            "NotSupportedError: Unable to create translator for the given "
            "source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);
}

// Tests the behavior of failing to load the library as a result of the
// incompatibility of the library.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorErrorLibraryIncompatible) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(1);
  mock_component_manager.InstallMockInvalidFunctionPointerLibraryComponent();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();

  auto console_observer =
      CreateConsoleObserver("The translation library is not compatible.");

  EXPECT_EQ(EvalJsCatchingError(R"(
            const translator = await Translator.create({
              sourceLanguage: 'en',
              targetLanguage: 'ja',
            });
      )"),
            "NotSupportedError: Unable to create translator for the given "
            "source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);
}

// Tests the behavior of failing to load the library as a result of the
// initialization failure of the library.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorErrorLibraryFailedToInitialize) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(1);
  mock_component_manager.InstallMockFailingLibraryComponent();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();

  auto console_observer =
      CreateConsoleObserver("Failed to initialize the translation library.");

  EXPECT_EQ(EvalJsCatchingError(R"(
            const translator = await Translator.create({
              sourceLanguage: 'en',
              targetLanguage: 'ja',
            });
      )"),
            "NotSupportedError: Unable to create translator for the given "
            "source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);
}

// Tests the behavior of failure of translator creation in the library.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorErrorLibraryTranslatorCreationFailed) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();

  auto console_observer = CreateConsoleObserver(
      "The translation library failed to create a translator.");

  EXPECT_EQ(EvalJsCatchingError(R"(
            const translator = await Translator.create({
              sourceLanguage: 'ja',
              targetLanguage: 'en',
            });
      )"),
            "NotSupportedError: Unable to create translator for the given "
            "source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);
}

// Tests progress monitor behavior.
class OnDeviceTranslationProgressMonitorBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationProgressMonitorBrowserTest() = default;
  ~OnDeviceTranslationProgressMonitorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    OnDeviceTranslationBrowserTest::SetUpOnMainThread();
    NavigateToEmptyPage();
    translation_manager_ = std::make_unique<MockTranslationManagerImpl>(
        GetBrowserContext(), GetLastCommittedOrigin());

    // Setup a ComponentUpdateService to be used by the TranslationManager.
    EXPECT_CALL(*translation_manager_, GetComponentUpdateService())
        .WillOnce(Invoke([&]() { return &component_update_service_; }));

    // `GetComponentIDs` should be called by the
    // `AIModelDownloadProgressManager` to filter out existing downloads.
    EXPECT_CALL(component_update_service_, GetComponentIDs()).Times(1);
  }

  void TearDownOnMainThread() override {
    translation_manager_.reset();
    OnDeviceTranslationBrowserTest::TearDownOnMainThread();
  }

  void TranslateAndMonitorProgress(std::string& source_language,
                                   std::string& target_language) {
    std::set<LanguagePackKey> language_pack_keys =
        CalculateRequiredLanguagePacks(source_language, target_language);

    base::RunLoop run_loop_translate_kit;
    EXPECT_CALL(component_manager_, RegisterTranslateKitComponentImpl())
        .WillOnce(Invoke([&]() { run_loop_translate_kit.Quit(); }));

    base::RunLoop run_loop_language_pack;
    EXPECT_CALL(component_manager_,
                RegisterTranslateKitLanguagePackComponent(_))
        .WillRepeatedly(Invoke([&](LanguagePackKey key) {
          EXPECT_EQ(language_pack_keys.erase(key), 1u);
          if (language_pack_keys.empty()) {
            run_loop_language_pack.Quit();
          }
        }));

    EXPECT_EQ(EvalJsCatchingError(base::StringPrintf(R"(
                  self.progressEvents = [];

                  self.createTranslatorPromise = Translator.create({
                    sourceLanguage: '%s',
                    targetLanguage: '%s',
                    monitor(m) {
                      m.addEventListener(
                          'downloadprogress', ({loaded, total}) => {
                            self.progressEvents.push({loaded, total});
                          });
                    },
                  });
                  return "OK";
                )",
                                                     source_language,
                                                     target_language)),
              "OK");

    run_loop_translate_kit.Run();
    run_loop_language_pack.Run();
  }

  AITestUtils::FakeComponent GetComponentForTranslateKit(uint64_t total_bytes) {
    return {component_updater::TranslateKitComponentInstallerPolicy::
                GetExtensionId(),
            total_bytes};
  }

  AITestUtils::FakeComponent GetComponentForLanguagePack(
      LanguagePackKey language_pack_key,
      uint64_t total_bytes) {
    const LanguagePackComponentConfig& config =
        GetLanguagePackComponentConfig(language_pack_key);
    std::string id =
        crx_file::id_util::GenerateIdFromHash(config.public_key_sha);
    return {id, total_bytes};
  }

  void SendUpdate(AITestUtils::FakeComponent component,
                  uint64_t downloaded_bytes) {
    component_update_service_.SendUpdate(component.CreateUpdateItem(
        update_client::ComponentState::kDownloading, downloaded_bytes));
  }

  double NormalizedProgress(uint64_t downloaded_bytes, uint64_t total_bytes) {
    // `AIUtils::NormalizeModelDownloadProgress` normalizes to 0 - 0x10000
    // range. We divide it by 0x10000 (65536) again to get it in the 0.0 - 1.0
    // range.
    return AIUtils::NormalizeModelDownloadProgress(downloaded_bytes,
                                                   total_bytes) /
           65536.0;
  }

  void FinishInstalling(std::string& source_language,
                        std::string& target_language) {
    component_manager_.InstallMockTranslateKitComponentLater();

    std::set<LanguagePackKey> language_pack_keys =
        CalculateRequiredLanguagePacks(source_language, target_language);
    for (LanguagePackKey language_pack_key : language_pack_keys) {
      component_manager_.InstallMockLanguagePackLater(language_pack_key);
    }
  }

  void ExpectUpdatesAre(const std::vector<double>& expected_updates) {
    base::Value::List actual_updates = EvalJs(R"((async () => {
                            await self.createTranslatorPromise;
                            return self.progressEvents;
                          })())")
                                           .ExtractList();

    ASSERT_EQ(actual_updates.size(), expected_updates.size());
    for (size_t i = 0; i < actual_updates.size(); i++) {
      auto& actual_update = actual_updates[i].GetDict();
      std::optional<double> actual_loaded = actual_update.FindDouble("loaded");
      std::optional<double> actual_total = actual_update.FindDouble("total");
      ASSERT_TRUE(actual_loaded);
      ASSERT_TRUE(actual_total);

      double expected_loaded = expected_updates[i];
      EXPECT_EQ(*actual_loaded, expected_loaded);
      EXPECT_EQ(*actual_total, 1);
    }
  }

 private:
  MockComponentManager component_manager_{GetTempDir()};
  AITestUtils::MockComponentUpdateService component_update_service_;
  std::unique_ptr<MockTranslationManagerImpl> translation_manager_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that progress events are received properly when translation requires
// one language pack.
//
// TODO(crbug.com/403592445): Add another test for when translation requires two
// language packs. It's not possible currently since the browser tests can't
// translate between two non-english languages.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationProgressMonitorBrowserTest,
                       ReceivesProgressEventsForOneLanguagePack) {
  std::string source_language = "en";
  std::string target_language = "ja";
  TranslateAndMonitorProgress(source_language, target_language);

  // Components we expect to receive updates for.
  AITestUtils::FakeComponent translation_kit =
      GetComponentForTranslateKit(4321);
  AITestUtils::FakeComponent en_ja_language_pack =
      GetComponentForLanguagePack(LanguagePackKey::kEn_Ja, 1234);

  // The downloaded bytes and total bytes for all components.
  uint64_t downloaded_bytes = 0;
  uint64_t total_bytes =
      translation_kit.total_bytes() + en_ja_language_pack.total_bytes();

  std::vector<double> expected_updates = {};

  // Receives the zero update.
  {
    expected_updates.emplace_back(0);

    SendUpdate(translation_kit, 0);
    SendUpdate(en_ja_language_pack, 0);
  }

  // Receives an update for translation kit normalized to the total_bytes.
  {
    Sleep(base::Milliseconds(100));

    uint64_t update_bytes = 999;
    downloaded_bytes += update_bytes;
    SendUpdate(translation_kit, update_bytes);

    expected_updates.emplace_back(
        NormalizedProgress(downloaded_bytes, total_bytes));
  }

  // Receives an update for the en ja language pack normalized to the
  // total_bytes.
  {
    Sleep(base::Milliseconds(100));

    uint64_t update_bytes = 300;
    downloaded_bytes += update_bytes;
    SendUpdate(en_ja_language_pack, update_bytes);

    expected_updates.emplace_back(
        NormalizedProgress(downloaded_bytes, total_bytes));
  }

  // Receives the final one update when all bytes are loaded.
  {
    SendUpdate(translation_kit, translation_kit.total_bytes());
    SendUpdate(en_ja_language_pack, en_ja_language_pack.total_bytes());

    expected_updates.emplace_back(1);
  }

  FinishInstalling(source_language, target_language);

  ExpectUpdatesAre(expected_updates);
}

// Confirms that `Translator.availability()` is not masked for a translation
// containing only English or the user's preferred languages.
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    TranslatorAvailabilityNotMasked_EnglishAndPreferredLanguages) {
  SetSelectedLanguages("fr");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  // The language pack for the preferred language is installed already.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Fr);

  // When the required language packs are installed for translation
  // between English and a preferred language, `availability()` is not masked
  // prior to initial translator creation.
  TestTranslationAvailable(browser(), "en", "fr", "available");
}

// Tests that `Translator.availability()` for a translation
// containing a language outside of English + the user's preferred languages.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       TranslatorAvailabilityMasked_ForNonPreferredLanguages) {
  SetSelectedLanguages("fr");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Fr);
  NavigateToEmptyPage();

  // Translation is not available for unsupported languages.
  TestTranslationAvailable(browser(), "fr", "abcxyz", "unavailable");

  // The Japanese language pack needs to be downloaded in order for
  // translation between English and Japanese to become "available".
  TestTranslationAvailable(browser(), "en", "ja", "downloadable");

  // After installing the en-ja language pack, `availability()` is still masked
  // as "downloadable", because Japanese is not one of the preferred languages,
  // and the site has not yet initialized a translator for that specific pair.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);
  TestTranslationAvailable(browser(), "en", "ja", "downloadable");

  // `availability()` is no longer masked as "downloadable" after creating an
  // initial translator for the given translation.
  TestSimpleTranslationWorks(browser(), "en", "ja");
  TestTranslationAvailable(browser(), "en", "ja", "available");

  // When site data is cleared (the stored values for
  // `ContentSettingsType::INTIALIZED_TRANSLATIONS` are cleared), the
  // translation `availability()` becomes masked once again.
  ClearSiteContentSettings();
  TestTranslationAvailable(browser(), "en", "ja", "downloadable");
}

// A delay is triggered for a "downloadable" translation containing a language
// outside of English + preferred languages.
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    CreateTranslator_Delay_ForMaskedDownloadableTranslation) {
  // Setup Translate Kit Component and select Spanish as the preferred language.
  SetSelectedLanguages("en,es");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  auto manager =
      MockTranslationManagerImpl(GetBrowserContext(), GetLastCommittedOrigin());

  // Simulate the download of an additional language pack (Japanese) by another
  // site.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // The delay is triggered upon the initial translator creation for Japanese,
  // given that it is not a preferred language.
  EXPECT_CALL(manager, GetTranslatorDownloadDelay()).Times(1);
  TestSimpleTranslationWorks(browser(), "en", "ja");

  // The delay does not occur on subsequent uses of the same language pair.
  EXPECT_CALL(manager, GetTranslatorDownloadDelay()).Times(0);
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// TODO(crbug.com/421947718): Disabled because there's a race between triggering
// user activation and consuming it when calling `create` multiple times.
//
// A delay is triggered when a second translator for a given translation is
// created during the delay time window of an initial translator's creation
// (which is also expected to trigger a delay).
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    DISABLED_CreateTranslator_Delay_ForTranslatorCreatedDuringInitialTranslatorCreationWithDelay) {
  SetSelectedLanguages("es");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  auto manager =
      MockTranslationManagerImpl(GetBrowserContext(), GetLastCommittedOrigin());

  // Simulate the download of an additional language pack (Japanese) by another
  // site.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  EXPECT_TRUE(ExecJs("self.createPromises = [];"));

  std::string create_translator_script = R"(
    self.createPromises.push(
        Translator.create({sourceLanguage: 'en', targetLanguage: 'ja'}));
  )";

  // The added delay should be triggered twice, once for each translator
  // creation.
  EXPECT_CALL(manager, GetTranslatorDownloadDelay()).Times(2);

  // Each call to `Translator.create` must be in a separate `ExecJs` call.
  // `Translator.create` consumes user activation if not english or preferred,
  // and `ExecJs` provides an initial user activation on each call.
  EXPECT_TRUE(ExecJs(create_translator_script));
  EXPECT_TRUE(ExecJs(create_translator_script));
  ASSERT_EQ(EvalJsCatchingError(R"(
    await Promise.all(self.createPromises);
    return 'OK';
  )"),
            "OK");
}

// `Translator.create` should still require user activation if the language pair
// is readily available but the site hasn't created a Translator for the
// language pair yet.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateRequiresUserActivationWhenDownloadedButMasked) {
  SetSelectedLanguages("es");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  // Simulate the download of an additional language pack (Japanese) by another
  // site.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  ASSERT_EQ(
      EvalJsCatchingError(R"(
    await Translator.create({sourceLanguage: 'en', targetLanguage: 'ja'});
    return 'OK';
  )",
                          browser(), content::EXECUTE_SCRIPT_NO_USER_GESTURE),
      "NotAllowedError: Requires a user gesture when availability is "
      "\"downloading\" or \"downloadable\".");
}

// No delay is triggered for a "downloadable" translation between English +
// preferred languages.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslator_NoDelay_DownloadableTranslation) {
  SetSelectedLanguages("en,es");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  auto manager =
      MockTranslationManagerImpl(GetBrowserContext(), GetLastCommittedOrigin());
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Es});
  EXPECT_CALL(manager, GetTranslatorDownloadDelay()).Times(0);
  TestSimpleTranslationWorks(browser(), "en", "es");

  // No delay is triggered now that the translation is "available".
  EXPECT_CALL(manager, GetTranslatorDownloadDelay()).Times(0);
  TestSimpleTranslationWorks(browser(), "en", "es");
}

// No delay is triggered in attempt to create a translator for an unsupported
// language.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslator_NoDelay_UnsupportedLanguage) {
  SetSelectedLanguages("en,xx");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  auto manager =
      MockTranslationManagerImpl(GetBrowserContext(), GetLastCommittedOrigin());

  EXPECT_CALL(manager, GetTranslatorDownloadDelay()).Times(0);
  EXPECT_NE(EvalJsCatchingError(R"(
      const translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'xx',
      });
      return await translator.translate('hello');
    )"),
            "en to xx: hello");
}

// Tests the behavior of the crash of calling create() and availability().
class OnDeviceTranslationCrashingLangBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationCrashingLangBrowserTest() {
    // Need to set TranslationAPIAcceptLanguagesCheck to false to use a fake
    // language code `crash` to trigger a crash.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kTranslationAPI,
          {{"TranslationAPIAcceptLanguagesCheck", "false"}}}},
        {});
  }
  ~OnDeviceTranslationCrashingLangBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationBrowserTest::SetUpCommandLine(command_line);
    // Need to set the language pack path to the command line to accept the
    // fake language code `crash`.
    command_line->AppendSwitchASCII(
        "translate-kit-packages",
        base::StrCat({"crash,ja,", GetTempDir().AsUTF8Unsafe()}));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the behavior of the crash of calling create().
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrashingLangBrowserTest,
                       CrashWhileCallingCreateTranslator) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  NavigateToEmptyPage();

  MockTranslationManagerImpl manager(GetBrowserContext(),
                                     GetLastCommittedOrigin());
  manager.SetCrashesAllowed(true);

  auto console_observer =
      CreateConsoleObserver("The translation service crashed.");

  // Tries to create a translator for the fake language code `crash`. This
  // causes a crash in the mock TranslateKit component. See comments in
  // mock_translate_kit_lib.cc.
  EXPECT_EQ(EvalJsCatchingError(R"(
            const translator = await Translator.create({
              sourceLanguage: 'crash',
              targetLanguage: 'ja',
            });
      )"),
            "NotSupportedError: Unable to create translator for the given "
            "source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);
}

// Tests the behavior of the crash of calling availability().
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrashingLangBrowserTest,
                       CrashWhileCallingCanTranslate) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();

  MockTranslationManagerImpl manager(GetBrowserContext(),
                                     GetLastCommittedOrigin());
  manager.SetCrashesAllowed(true);

  // Tries to call availability() for the fake language code `crash`. This
  // causes a crash in the mock TranslateKit component. See comments in
  // mock_translate_kit_lib.cc.
  TestCanTranslateResult("crash", "ja",
                         CanCreateTranslatorResult::kNoServiceCrashed);
}

IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest, NoExistFileHandling) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToEmptyPage();

  EXPECT_EQ(EvalJsCatchingError(R"(
      const translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'ja',
      });
      return await translator.translate('CHECK_NOT_EXIST_FILE');
  )"),
            "Result of Open(): Failed, result of FileExists(): false, "
            "is_directory: false");
}

// Tests the behavior of create() when the accept language check
// fails.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorAcceptLanguagesCheckFailed) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.DoNotExpectCallRegisterTranslateKitComponent();
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();

  NavigateToEmptyPage();

  auto console_observer =
      CreateConsoleObserver("The language pair is unsupported.");

  // Create a translator for unsupported language.
  TestCreateTranslator(browser(), "en", "xx",
                       "NotSupportedError: Unable to create translator for the "
                       "given source and target language.");

  // The console message should be logged.
  WaitForConsoleObserver(*console_observer);
}

// Tests that the browser process can handle the case that the frame is deleted
// while creating a translator.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       FrameDeletedWhileCreatingATranslator) {
  MockComponentManager mock_component_manager(GetTempDir());

  base::RunLoop run_loop_for_register_translate_kit;
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .WillOnce(Invoke([&]() { run_loop_for_register_translate_kit.Quit(); }));

  base::RunLoop run_loop_for_register_language_pack;
  EXPECT_CALL(mock_component_manager,
              RegisterTranslateKitLanguagePackComponent(_))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Ja);
        run_loop_for_register_language_pack.Quit();
      }));

  NavigateToEmptyPage();

  content::RenderFrameHost* iframe = CreateIframe();

  // Create a translator in an iframe.
  EXPECT_EQ(content::EvalJs(iframe, R"(
      Translator.create({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
      'OK';
    )")
                .ExtractString(),
            "OK");
  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called.
  run_loop_for_register_language_pack.Run();

  // Deletes the iframe after the browser process receives the request.
  EXPECT_TRUE(RemoveIframe());

  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();
  // Install the mock language pack.
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // Test that Translator API works even after the frame requesting a translator
  // is deleted.
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// Tests that the service is terminated when the idle timeout is reached.
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    TerminateServiceWhenIdleTimeoutReachedAfterTranslatorDestroyed) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToEmptyPage();

  auto service_controller =
      ServiceControllerManager::GetForBrowserContext(browser()->profile())
          ->GetServiceControllerForOrigin(
              embedded_https_test_server().GetOrigin());

  // Set the idle timeout to be 100 microseconds.
  service_controller->SetServiceIdleTimeoutForTesting(base::Microseconds(100));

  // Test that Translator API works.
  EXPECT_EQ(EvalJsCatchingError(R"(
      window._translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'ja',
      });
      return await window._translator.translate('hello');
    )"),
            "en to ja: hello");
  // Check that the service is still running.
  EXPECT_TRUE(service_controller->IsServiceRunning());
  // Wait for 200 microseconds.
  EXPECT_EQ(EvalJsCatchingError(R"(
      await new Promise(resolve => { setTimeout(resolve, 200); });
      return 'OK';
    )"),
            "OK");
  // Check that the service is still running, because the translator is still
  // available.
  EXPECT_TRUE(service_controller->IsServiceRunning());
  // Destroy the translator. And wait for 200 microseconds. (Note: wait more
  // than the idle timeout 100 microseconds to avoid flakiness.)
  EXPECT_EQ(EvalJsCatchingError(R"(
      window._translator.destroy();
      await new Promise(resolve => { setTimeout(resolve, 200); });
      return 'OK';
    )"),
            "OK");
  // Check that the service is not running, because the translator was
  // destroyed, and the idle timeout was reached.
  EXPECT_FALSE(service_controller->IsServiceRunning());
}

// Tests that the service is terminated when the idle timeout is reached after
// the frame is removed.
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    TerminateServiceWhenIdleTimeoutReachedAfterFrameRemoved) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToEmptyPage();

  auto service_controller =
      ServiceControllerManager::GetForBrowserContext(browser()->profile())
          ->GetServiceControllerForOrigin(
              embedded_https_test_server().GetOrigin());
  // Set the idle timeout to be 100 microseconds.
  service_controller->SetServiceIdleTimeoutForTesting(base::Microseconds(100));

  content::RenderFrameHost* iframe = CreateIframe();

  // Test that Translator API on an iframe works.
  EXPECT_EQ(content::EvalJs(iframe, R"(
     (async () => {
        const translator =
            await Translator.create({
              sourceLanguage: 'en',
              targetLanguage: 'ja',
            });
        return await translator.translate('hello');
      })();
    )"),
            "en to ja: hello");
  // Check that the service is still running.
  EXPECT_TRUE(service_controller->IsServiceRunning());
  // Wait for 200 microseconds.
  EXPECT_EQ(EvalJsCatchingError(R"(
      await new Promise(resolve => { setTimeout(resolve, 200); });
      return 'OK';
    )"),
            "OK");
  // Check that the service is still running, because the ifame is still
  // available.
  EXPECT_TRUE(service_controller->IsServiceRunning());
  // Remove the iframe and wait for 200 microseconds. (Note: wait more than the
  // idle timeout 100 microseconds to avoid flakiness.)
  EXPECT_EQ(EvalJsCatchingError(R"(
      document.body.removeChild(window._iframe);
      await new Promise(resolve => { setTimeout(resolve, 200); });
      return 'OK';
    )"),
            "OK");
  // Check that the service is not running, because the iframe was removed, and
  // the idle timeout was reached.
  EXPECT_FALSE(service_controller->IsServiceRunning());
}

// Test the behavior of availability() when the language pack is ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest, CanTranslateReadily) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);

  // Despite being ready, the availability will be masked since the site hasn't
  // created a translator for this language pair yet.
  // `kAfterDownloadTranslatorCreationRequired` is only ever returned in that
  // situation, so receiving that value confirms that the package is readily
  // available.
  TestCanTranslateResult(
      "en", "ja",
      CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired);
}

// Test the behavior of availability() when the language pack is not ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CanTranslateAfterDownloadLanguagePackNotReady) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockTranslateKitComponent();
  TestCanTranslateResult(
      "en", "ja",
      CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady);
}

// Test the behavior of availability() when both the library and the language
// pack are not ready.
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    CanTranslateAfterDownloadLibraryAndLanguagePackNotReady) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  TestCanTranslateResult(
      "en", "ja",
      CanCreateTranslatorResult::kAfterDownloadLibraryAndLanguagePackNotReady);
}

// Test the behavior of availability() when the language pack is ready, but the
// library is not ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CanTranslateAfterDownloadLibraryNotReady) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);
  TestCanTranslateResult(
      "en", "ja", CanCreateTranslatorResult::kAfterDownloadLibraryNotReady);
}

// Test the behavior of availability() when the language is not supported.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CanTranslateNoNotSupportedLanguage) {
  // This test case uses English as the source language and an unsupported
  // language code as the target language. To avoid the failure of
  // PassAcceptLanguagesCheck(), we set the SelectedLanguages to be English and
  // the unsupported language code.
  SetSelectedLanguages("en,xx");
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  TestCanTranslateResult("en", "xx",
                         CanCreateTranslatorResult::kNoNotSupportedLanguage);
}

// Test the behavior of `availability()` when the execution context is not
// valid.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_InvalidStateError) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();
  EXPECT_EQ(EvalJsCatchingError(R"(
        const iframe = document.createElement('iframe');
        const loadPromise = new Promise(resolve => {
          iframe.addEventListener('load', resolve);
        });
        iframe.src = location.href;
        document.body.appendChild(iframe);
        await loadPromise;
        const iframeTranslator = iframe.contentWindow.Translator;
        document.body.removeChild(iframe);
        await iframeTranslator.availability({
          sourceLanguage: "en",
          targetLanguage: "es"
        });
      )"),
            "InvalidStateError: Failed to execute 'availability' on "
            "'Translator': The execution context is not valid.");
}

// Test the behavior of `availability()` for an unsupported language.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_Unavailable_NotSupportedLanguage) {
  // Set the unsupported language as a preferred language in order to avoid
  // `PassAcceptLanguagesCheck()` failure, for testing purposes.
  SetSelectedLanguages("en,xx");
  MockComponentManager mock_component_manager(GetTempDir());
  NavigateToEmptyPage();
  TestTranslationAvailable(browser(), "en", "xx", "unavailable");
}

// Test the behavior of `availability()` where the source language and the
// target language are the same language.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_Unavailable_SameSourceAndTargetLanguage) {
  SetSelectedLanguages("en,ja");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);
  NavigateToEmptyPage();
  TestTranslationAvailable(browser(), "ja", "ja", "unavailable");
  TestTranslationAvailable(browser(), "en", "en", "unavailable");
}

// Test the behavior of `availability()` when both the library and the language
// packs are not ready.
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBrowserTest,
    Availability_Downloadable_LibraryAndLanguagePackNotReady) {
  SetSelectedLanguages("en,ja");
  MockComponentManager mock_component_manager(GetTempDir());
  NavigateToEmptyPage();
  TestTranslationAvailable(browser(), "en", "ja", "downloadable");
}

// Test the behavior of `availability()` when the library is not ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_Downloadable_LibraryNotReady) {
  SetSelectedLanguages("en,es");
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  NavigateToEmptyPage();
  TestTranslationAvailable(browser(), "en", "es", "downloadable");
}

// Test the behavior of `availability()` when the library is ready, and
// language packs are not ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_Downloadable_LanguagePacksNotReady) {
  SetSelectedLanguages("en,es");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  NavigateToEmptyPage();
  TestTranslationAvailable(browser(), "en", "es", "downloadable");
}

IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_Available_ForSelectedPopularAndNonPopular) {
  SetSelectedLanguages("ja,fr");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Fr);
  NavigateToEmptyPage();

  TestTranslationAvailable(browser(), "ja", "fr", "available");
}

// Test the behavior of `availability()` when both the library and language
// packs are ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       Availability_Available_LibraryAndLanguagePackReady) {
  SetSelectedLanguages("en,ja");
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ja);
  NavigateToEmptyPage();
  TestTranslationAvailable(browser(), "en", "ja", "available");
}

// Test that calling both the legacy and new API works.
// This is a regression test for https://crbug.com/381344025.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest, UseBothLegacyAndNewAPI) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToEmptyPage();

  // Test that Translator legacy API works.
  EXPECT_EQ(EvalJsCatchingError(R"(
      const translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'ja',
      });
      return await translator.translate('hello');
    )"),
            "en to ja: hello");

  // Test that Translator new API works.
  EXPECT_EQ(EvalJsCatchingError(R"(
      const translator = await Translator.create({
        sourceLanguage: 'en',
        targetLanguage: 'ja',
      });
      return await translator.translate('hello');
    )"),
            "en to ja: hello");
}

// TODO(crbug.com/410842873): Add test to check behavior for extension
// service workers once supported.
//
// Test the behavior of the Translator API accessed from a service worker
// outside of an extension.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       APIAvailability_NonExtensionWorkers) {
  const std::string kWorkerScript =
      "try {"
      "    Translator;"
      "    self.postMessage('test');"
      "} catch (e) {"
      "    self.postMessage(e.name);"
      "}";

  base::RunLoop loop;
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &RespondWithJS, "/js-response", kWorkerScript, loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/workers/create_dedicated_worker.html?worker_url=/js-response")));
  loop.Run();

  EXPECT_EQ(
      "ReferenceError",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "waitForMessage();"));
}

// Test the behavior of availability() when PassAcceptLanguagesCheck() checks
// is skipped.
class OnDeviceTranslationSkipAcceptLanguagesCheckBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationSkipAcceptLanguagesCheckBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kTranslationAPI,
          {{"TranslationAPIAcceptLanguagesCheck", "false"}}}},
        {});
  }
  ~OnDeviceTranslationSkipAcceptLanguagesCheckBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test the behavior of availability() when PassAcceptLanguagesCheck() checks
// is skipped.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationSkipAcceptLanguagesCheckBrowserTest,
                       CanTranslateAcceptLanguagesCheckSkipped) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.InstallMockLanguagePack(LanguagePackKey::kEn_Ko);
  // Despite being ready, the availability will be masked since the site hasn't
  // created a translator for this language pair yet.
  // `kAfterDownloadTranslatorCreationRequired` is only ever returned in that
  // situation, so receiving that value confirms that the package is readily
  // available.
  TestCanTranslateResult(
      "en", "ko",
      CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired);
}

// Test the behavior of Translator API in a cross origin iframe.
class OnDeviceTranslationCrossOriginBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationCrossOriginBrowserTest() {
    // Use a reduced value for TranslationAPIMaxServiceCount to speed
    // up the test.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kTranslationAPI,
          {{"TranslationAPIMaxServiceCount", "2"}}}},
        {});
    CHECK_EQ(kTranslationAPIMaxServiceCount.Get(), 2u);
  }
  ~OnDeviceTranslationCrossOriginBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Need to use URLLoaderInterceptor to handle requests to multiple origins.
    url_loader_interceptor_.emplace(base::BindRepeating(
        &OnDeviceTranslationCrossOriginBrowserTest::InterceptRequest));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

 protected:
  // Creates a URL for an iframe with a unique origin.
  static GURL CreateCrossOriginIframeUrl(size_t index) {
    return GURL(
        base::StringPrintf("https://test-%zd.example/frame.html", index));
  }

  // Navigates to the test page.
  void NavigateToTestPage(Browser* target_browser) {
    CHECK(ui_test_utils::NavigateToURL(
        target_browser ? target_browser : browser(),
        GURL("https://translation-api.test/index.html")));
  }

  // Adds an iframe to the test page and optionally sets its permission policy.
  content::RenderFrameHost* AddIframe(size_t index,
                                      Browser* target_browser,
                                      bool permission_policy_enabled) {
    EXPECT_EQ(EvalJsCatchingError(JsReplace("return addIframe($1, $2);",
                                            CreateCrossOriginIframeUrl(index),
                                            permission_policy_enabled),
                                  target_browser),
              "loaded");

    return ChildFrameAt((target_browser ? target_browser : browser())
                            ->tab_strip_model()
                            ->GetActiveWebContents(),
                        index);
  }

  // Removes the iframe and waits for the service deletion.
  void RemoveIframeAndWaitForServiceDeletion(size_t index,
                                             Browser* target_browser) {
    base::RunLoop run_loop;
    ServiceControllerManager::GetForBrowserContext(target_browser->profile())
        ->set_service_controller_deleted_observer_for_testing(
            run_loop.QuitClosure());
    EXPECT_EQ(EvalJsCatchingError(JsReplace("return removeIframe($1);",
                                            CreateCrossOriginIframeUrl(index)),
                                  target_browser),
              "removed");
    run_loop.Run();
  }

  // Creates a translator and translates in the iframe. Returns successful
  // translation or the error message.
  std::string CheckTranslateInIframe(content::RenderFrameHost* iframe) {
    const std::string_view translateTestScript = R"(
        (async () => {
          try {
            window._translator = await Translator.create({
              sourceLanguage: 'en',
              targetLanguage: 'ja',
            });
            if (window._translator) {
              return await window._translator.translate('hello');
            }
          } catch (e) {
            return e.name.toString();
          }
        })()
      )";
    return content::EvalJs(iframe, translateTestScript).ExtractString();
  }

  // Checks the result of availability() in the iframe.
  std::string TryCanTranslateInIframe(content::RenderFrameHost* iframe) {
    const std::string_view translateTestScript = R"(
      (async () => {
        try {
          return await Translator.availability({
            sourceLanguage: 'en',
            targetLanguage: 'ja',
          });
        } catch (e) {
          return e.name.toString();
        }
      })()
    )";
    return content::EvalJs(iframe, translateTestScript).ExtractString();
  }

 private:
  // URLLoaderInterceptor callback
  static bool InterceptRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.path() == "/index.html") {
      content::URLLoaderInterceptor::WriteResponse(
          "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n\n",
          R"(
          <head><script>
          const frames = {};
          async function addIframe(url, permissionPolicyEnabled) {
            const frame = document.createElement('iframe');
            frames[url] = frame;
            if (permissionPolicyEnabled) {
              frame.setAttribute('allow', 'translator');
            }
            const loadedPromise = new Promise(resolve => {
              frame.addEventListener('load', () => {
                resolve('loaded');
              });
            });
            frame.src = url;
            document.body.appendChild(frame);
            return await loadedPromise;
          }
          async function evalInIframe(url, script) {
            const frame = frames[url];
            const channel = new MessageChannel();
            const onMessagePromise = new Promise(resolve => {
              channel.port1.addEventListener('message', e => {
                resolve(e.data);
              });
            });
            channel.port1.start();
            frame.contentWindow.postMessage(script, '*', [channel.port2]);
            return await onMessagePromise;
          }
          function removeIframe(url) {
            document.body.removeChild(frames[url]);
            return 'removed';
          }
          </script></head>)",
          params->client.get(),
          /*ssl_info=*/std::nullopt, params->url_request.url);
      return true;
    } else if (params->url_request.url.path() == "/frame.html") {
      content::URLLoaderInterceptor::WriteResponse(
          "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n\n",
          R"(
          <head><script>
          window.addEventListener('message', async (e) => {
            e.ports[0].postMessage(await eval(e.data));
          });
          </script></head>)",
          params->client.get(),
          /*ssl_info=*/std::nullopt, params->url_request.url);
      return true;
    }
    return false;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Tests the behavior of the Translation API in a cross origin iframe.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrossOriginBrowserTest,
                       TranslateInCrossOriginIframe) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);

  NavigateToTestPage(browser());
  content::RenderFrameHost* iframe =
      AddIframe(0, browser(), /*enable_permission_policy=*/false);

  // Translation is not available in cross-origin iframes without permission
  // policy.
  EXPECT_EQ(CheckTranslateInIframe(iframe), "NotAllowedError");
}

// Tests the behavior of the Translation API in a cross origin iframe when the
// service count exceeds the limit.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrossOriginBrowserTest,
                       ExceedServiceCountLimit) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  NavigateToTestPage(browser());
  size_t i = 0;
  // Until the service count exceeds the limit, the translator can be created,
  // and the translation is successful.
  for (; i < kTranslationAPIMaxServiceCount.Get(); i++) {
    content::RenderFrameHost* iframe =
        AddIframe(i, browser(), /*enable_permission_policy=*/true);
    EXPECT_EQ(CheckTranslateInIframe(iframe), "en to ja: hello");
    EXPECT_EQ(TryCanTranslateInIframe(iframe), "available");
  }

  // When the service count exceeds the limit, the translator cannot be created,
  // even when the permission policy is still enabled.
  content::RenderFrameHost* iframe =
      AddIframe(i, browser(), /*enable_permission_policy=*/true);
  auto console_observer = CreateConsoleObserver(
      "The translation service count exceeded the limitation.");
  EXPECT_EQ(CheckTranslateInIframe(iframe), "NotSupportedError");
  WaitForConsoleObserver(*console_observer);
  EXPECT_EQ(TryCanTranslateInIframe(iframe), "unavailable");

  // When the service count is back to under the limit, the translator can be
  // created again.
  RemoveIframeAndWaitForServiceDeletion(0, browser());
  EXPECT_EQ(CheckTranslateInIframe(iframe), "en to ja: hello");
  EXPECT_EQ(TryCanTranslateInIframe(iframe), "available");
}

// Tests the behavior of the Translation API in a cross origin iframe using the
// incognito profile.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrossOriginBrowserTest,
                       TranslateInIframeIncognitoBrowser) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  Browser* incognito_browser = CreateIncognitoBrowser();

  NavigateToTestPage(incognito_browser);
  content::RenderFrameHost* iframe0 =
      AddIframe(0, incognito_browser, /*enable_permission_policy=*/true);
  EXPECT_EQ(CheckTranslateInIframe(iframe0), "en to ja: hello");

  content::RenderFrameHost* iframe1 =
      AddIframe(1, incognito_browser, /*enable_permission_policy=*/false);
  EXPECT_EQ(CheckTranslateInIframe(iframe1), "NotAllowedError");
}

// Tests the behavior of the Translation API in a cross origin iframe using the
// guest profile.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrossOriginBrowserTest,
                       TranslateInIframeGuestBrowser) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  Browser* guest_browser = CreateGuestBrowser();

  NavigateToTestPage(guest_browser);
  content::RenderFrameHost* iframe =
      AddIframe(0, guest_browser, /*enable_permission_policy=*/true);
  EXPECT_EQ(CheckTranslateInIframe(iframe), "en to ja: hello");
}

// TODO(crbug.com/423029203): This is timing out on a CQ bot so it is disabled
// for now until we can resolve that issue.
//
// Tests the behavior of the Translation API in a cross origin iframe using
// multiple profiles.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrossOriginBrowserTest,
                       DISABLED_ServiceCountLimitIsolatedPerProfile) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath other_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  // Create an additional profile.
  Profile& additional_profile =
      profiles::testing::CreateProfileSync(profile_manager, other_path);

  std::vector<Browser*> browsers = {
      browser(),
      CreateBrowser(&additional_profile),
      CreateIncognitoBrowser(),
      CreateGuestBrowser(),
  };

  for (auto* target_browser : browsers) {
    NavigateToTestPage(target_browser);
  }

  // Create translators and translate in each profile. Until the service count
  // per profile exceeds the limit, the translator can be created, and the
  // translation is successful.
  for (size_t i = 0; i < kTranslationAPIMaxServiceCount.Get(); i++) {
    for (auto* target_browser : browsers) {
      content::RenderFrameHost* iframe =
          AddIframe(i, target_browser, /*enable_permission_policy=*/true);
      EXPECT_EQ(CheckTranslateInIframe(iframe), "en to ja: hello");
    }
  }

  const size_t limit_count = kTranslationAPIMaxServiceCount.Get();

  std::vector<content::RenderFrameHost*> iframes;

  // When the service count per profile exceeds the limit, the translator
  // cannot be created.
  for (auto* target_browser : browsers) {
    content::RenderFrameHost* iframe = AddIframe(
        limit_count, target_browser, /*enable_permission_policy=*/true);
    iframes.push_back(iframe);
    auto console_observer = CreateConsoleObserver(
        "The translation service count exceeded the limitation.",
        target_browser);
    EXPECT_EQ(CheckTranslateInIframe(iframe), "NotSupportedError");
    // The console message should be logged.
    WaitForConsoleObserver(*console_observer);
  }

  ASSERT_EQ(iframes.size(), browsers.size());

  // When the service count per profile is back to under the limit, the
  // translator can be created again.
  for (size_t i = 0; i < browsers.size(); i++) {
    Browser* target_browser = browsers[i];
    content::RenderFrameHost* iframe = iframes[i];
    RemoveIframeAndWaitForServiceDeletion(0, target_browser);
    EXPECT_EQ(CheckTranslateInIframe(iframe), "en to ja: hello");
  }
}

// Tests the behavior of the Translation API in a cross origin iframe using
// the command line. We need this test because the implementation of
// availability() is different when the command line is used.
class OnDeviceTranslationCrossOriginWithCommandLineBrowserTest
    : public OnDeviceTranslationCrossOriginBrowserTest {
 public:
  OnDeviceTranslationCrossOriginWithCommandLineBrowserTest() = default;
  ~OnDeviceTranslationCrossOriginWithCommandLineBrowserTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationCrossOriginBrowserTest::SetUpCommandLine(command_line);
    SetMockLibraryPathToCommandLine(command_line);
    WriteFakeDictionaryDataAndSetCommandLine(LanguagePackKey::kEn_Ja,
                                             GetTempDir(), command_line);
  }
};

// Tests the behavior of the Translation API in a cross origin iframe when the
// service count exceeds the limit.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCrossOriginWithCommandLineBrowserTest,
                       ExceedServiceCountLimit) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.DoNotExpectCallRegisterTranslateKitComponent();
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();

  NavigateToTestPage(browser());
  size_t i = 0;
  // Until the service count exceeds the limit, the translator can be created,
  // and the translation is successful.
  for (; i < kTranslationAPIMaxServiceCount.Get(); i++) {
    content::RenderFrameHost* iframe =
        AddIframe(i, browser(), /*enable_permission_policy=*/true);
    EXPECT_EQ(CheckTranslateInIframe(iframe), "en to ja: hello");
    EXPECT_EQ(TryCanTranslateInIframe(iframe), "available");
  }

  // When the service count exceeds the limit, the translator cannot be created.
  content::RenderFrameHost* last_iframe =
      AddIframe(i, browser(), /*enable_permission_policy=*/true);
  EXPECT_EQ(CheckTranslateInIframe(last_iframe), "NotSupportedError");
  EXPECT_EQ(TryCanTranslateInIframe(last_iframe), "unavailable");

  // When the service count is back to under the limit, the translator can be
  // created again.
  RemoveIframeAndWaitForServiceDeletion(0, browser());
  EXPECT_EQ(CheckTranslateInIframe(last_iframe), "en to ja: hello");
  EXPECT_EQ(TryCanTranslateInIframe(last_iframe), "available");
}

// Tests the behavior of when the command line flag "translate-kit-binary-path"
// is provided.
class OnDeviceTranslationBinaryPathCommandLineBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationBinaryPathCommandLineBrowserTest() = default;
  ~OnDeviceTranslationBinaryPathCommandLineBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationBrowserTest::SetUpCommandLine(command_line);
    SetMockLibraryPathToCommandLine(command_line);
  }
};
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBinaryPathCommandLineBrowserTest,
                       SimpleTranslation) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.DoNotExpectCallRegisterTranslateKitComponent();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// Tests the behavior of when the command line flag "translate-kit-packages"
// is provided.
class OnDeviceTranslationPackagesCommandLineBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationPackagesCommandLineBrowserTest() = default;
  ~OnDeviceTranslationPackagesCommandLineBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationBrowserTest::SetUpCommandLine(command_line);
    WriteFakeDictionaryDataAndSetCommandLine(LanguagePackKey::kEn_Ja,
                                             GetTempDir(), command_line);
  }
};
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationPackagesCommandLineBrowserTest,
                       SimpleTranslation) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();
  NavigateToEmptyPage();
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// Tests the behavior of availability() when the required language package
// is provided by the command line flag.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationPackagesCommandLineBrowserTest,
                       CanTranslateReadily) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();
  NavigateToEmptyPage();

  // Despite being ready, the availability will be masked since the site hasn't
  // created a translator for this language pair yet.
  // `kAfterDownloadTranslatorCreationRequired` is only ever returned in that
  // situation, so receiving that value confirms that the package is readily
  // available.
  TestCanTranslateResult(
      "en", "ja",
      CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired);
}

// Tests the behavior of availability() when the required language package
// is not provided by the command line flag.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationPackagesCommandLineBrowserTest,
                       CanTranslateNoNotSupportedLanguage) {
  // This test case uses English as the source language and French as the target
  // language. To avoid the failure of PassAcceptLanguagesCheck(), we set the
  // SelectedLanguages to be English and French.
  SetSelectedLanguages("en,fr");
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.InstallMockTranslateKitComponent();
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();
  NavigateToEmptyPage();
  TestCanTranslateResult("en", "fr",
                         CanCreateTranslatorResult::kNoNotSupportedLanguage);
}

// Tests the behavior of availability() when the required language package
// is provided by the command line flag, but the library is not ready.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationPackagesCommandLineBrowserTest,
                       CanTranslateAfterDownloadLibraryNotReady) {
  MockComponentManager mock_component_manager(GetTempDir());
  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(0);
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();
  NavigateToEmptyPage();
  TestCanTranslateResult(
      "en", "ja", CanCreateTranslatorResult::kAfterDownloadLibraryNotReady);
}

// Tests the behavior of when the command line flags "translate-kit-binary-path"
// and "translate-kit-packages" are provided.
class OnDeviceTranslationBinaryPathAndPackagesCommandLineBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationBinaryPathAndPackagesCommandLineBrowserTest() = default;
  ~OnDeviceTranslationBinaryPathAndPackagesCommandLineBrowserTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationBrowserTest::SetUpCommandLine(command_line);
    SetMockLibraryPathToCommandLine(command_line);
    WriteFakeDictionaryDataAndSetCommandLine(LanguagePackKey::kEn_Ja,
                                             GetTempDir(), command_line);
  }
};
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationBinaryPathAndPackagesCommandLineBrowserTest,
    SimpleTranslation) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.DoNotExpectCallRegisterTranslateKitComponent();
  mock_component_manager.DoNotExpectCallRegisterLanguagePackComponent();
  NavigateToEmptyPage();
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// Tests the behavior of when the number of values passed to the
// "translate-kit-packages" command-line flag is not a multiple of three.
class OnDeviceTranslationInvalidCommandLineNonTriadPackageFlagBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationInvalidCommandLineNonTriadPackageFlagBrowserTest() =
      default;
  ~OnDeviceTranslationInvalidCommandLineNonTriadPackageFlagBrowserTest()
      override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationBrowserTest::SetUpCommandLine(command_line);
    // This is an invalid flag as "translate-kit-packages" requires
    // arguments in groups of three: "first language code, second language code,
    // model path".
    command_line->AppendSwitchASCII("translate-kit-packages", "en,ja");
  }
};
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationInvalidCommandLineNonTriadPackageFlagBrowserTest,
    SimpleTranslation) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

// Tests the behavior of when non-ASCII language code is passed to the
// "translate-kit-packages" command-line flag.
class OnDeviceTranslationInvalidCommandLineNonAsciiLanguageFlagBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationInvalidCommandLineNonAsciiLanguageFlagBrowserTest() =
      default;
  ~OnDeviceTranslationInvalidCommandLineNonAsciiLanguageFlagBrowserTest()
      override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OnDeviceTranslationBrowserTest::SetUpCommandLine(command_line);
    // This is an invalid flag as "translate-kit-packages" requires
    // arguments in groups of three: "first language code, second language code,
    // model path", and the language codes must be ASCII.
    command_line->AppendSwitchASCII(
        "translate-kit-packages",
        base::StrCat({"en,日本語,", GetTempDir().AsUTF8Unsafe()}));
  }
};
IN_PROC_BROWSER_TEST_F(
    OnDeviceTranslationInvalidCommandLineNonAsciiLanguageFlagBrowserTest,
    SimpleTranslation) {
  MockComponentManager mock_component_manager(GetTempDir());
  mock_component_manager.ExpectCallRegisterTranslateKitComponentAndInstall();
  mock_component_manager.ExpectCallRegisterLanguagePackComponentAndInstall(
      {LanguagePackKey::kEn_Ja});
  NavigateToEmptyPage();
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

}  // namespace on_device_translation
