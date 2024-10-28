// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/test/test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features_generated.h"

using ::testing::_;
using ::testing::Invoke;

namespace on_device_translation {

namespace {

std::string CreateFakeDictionaryData(const std::string& sourceLang,
                                     const std::string& targetLang) {
  return base::StringPrintf("%s to %s: ", sourceLang, targetLang);
}

class MockComponentManager : public ComponentManager {
 public:
  explicit MockComponentManager(const base::FilePath& package_dir)
      : package_dir_(package_dir) {
    ComponentManager::SetForTesting(this);
  }

  ~MockComponentManager() override { ComponentManager::SetForTesting(nullptr); }

  // Disallow copy and assign.
  MockComponentManager(const MockComponentManager&) = delete;
  MockComponentManager& operator=(const MockComponentManager&) = delete;

  // ComponentManager implements:
  MOCK_METHOD(void, RegisterTranslateKitComponentImpl, (), (override));
  MOCK_METHOD(void,
              RegisterTranslateKitLanguagePackComponent,
              (LanguagePackKey),
              (override));
  MOCK_METHOD(void,
              UninstallTranslateKitLanguagePackComponent,
              (LanguagePackKey),
              (override));
  base::FilePath GetTranslateKitComponentPathImpl() override {
    return package_dir_;
  }

  // Installs the mock TranslateKit component.
  // The mock component's translate method returns the concatenation of the
  // content of "dict.dat" in the language pack and the input text.
  // See comments in mock_translate_kit_lib.cc for more details.
  void InstallMockTranslateKitComponent() {
    base::ScopedAllowBlockingForTesting allow_io;
    const auto mock_library_path = GetMockLibraryPath();
    const auto binary_path = package_dir_.Append(mock_library_path.BaseName());
    if (!base::DirectoryExists(binary_path.DirName())) {
      CHECK(base::CreateDirectory(binary_path.DirName()));
    }
    CHECK(base::CopyFile(mock_library_path, binary_path));
    g_browser_process->local_state()->SetFilePath(
        prefs::kTranslateKitBinaryPath, binary_path);
  }

  // Installs the mock language pack.
  void InstallMockLanguagePack(LanguagePackKey language_pack_key,
                               const std::string& fake_dictionary_data) {
    base::ScopedAllowBlockingForTesting allow_io;
    const auto dict_dir_path =
        package_dir_.AppendASCII(GetPackageInstallDirName(language_pack_key));
    const auto dict_path = dict_dir_path.AppendASCII("dict.dat");
    if (!base::DirectoryExists(dict_dir_path)) {
      CHECK(base::CreateDirectory(dict_dir_path));
    }
    CHECK(
        base::File(dict_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
            .WriteAndCheck(0, base::as_byte_span(fake_dictionary_data)));
    const LanguagePackComponentConfig* config =
        kLanguagePackComponentConfigMap.at(language_pack_key);
    g_browser_process->local_state()->SetBoolean(
        GetRegisteredFlagPrefName(*config), true);
    g_browser_process->local_state()->SetFilePath(
        GetComponentPathPrefName(*config), dict_dir_path);
  }

  // Post a task to call InstallMockLanguagePack()
  void InstallMockLanguagePackLater(
      LanguagePackKey language_pack_key,
      const std::string_view fake_dictionary_data) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockComponentManager::InstallMockLanguagePack,
                       weak_ptr_factory_.GetWeakPtr(), language_pack_key,
                       std::string(fake_dictionary_data)));
  }

 private:
  const base::FilePath package_dir_;
  base::WeakPtrFactory<MockComponentManager> weak_ptr_factory_{this};
};

void TestSimpleTranslationWorks(Browser* browser,
                                const std::string& sourceLang,
                                const std::string& targetLang) {
  // Translate "hello" from `sourceLang` to `targetLang`.
  // Note: the mock TranslateKit component returns the concatenation of the
  // content of "dict.dat" in the language pack and the input text.
  // See comments in mock_translate_kit_lib.cc for more details.
  EXPECT_EQ(EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                   base::StringPrintf(R"(
        (async () => {
          try {
            const translator = await translation.createTranslator({
              sourceLanguage: '%s',
              targetLanguage: '%s',
            });
            return await translator.translate('hello');
          } catch (e) {
            return e.toString();
          }
        })();
      )",
                                      sourceLang, targetLang))
                .ExtractString(),
            base::StringPrintf("%s to %s: hello", sourceLang, targetLang));
}

void TestCreateTranslator(Browser* browser,
                          const std::string& sourceLang,
                          const std::string& targetLang,
                          const std::string& result) {
  ASSERT_EQ(EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                   base::StringPrintf(R"(
  (async () => {
    try {
      await translation.createTranslator({
          sourceLanguage: '%s',
          targetLanguage: '%s',
        });
      return 'OK';
    } catch (e) {
      return e.toString();
    }
    })();
  )",
                                      sourceLang, targetLang))
                .ExtractString(),
            result);
}

}  // namespace

class OnDeviceTranslationBrowserTest : public InProcessBrowserTest {
 public:
  OnDeviceTranslationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kEnableTranslationAPI);
    CHECK(tmp_dir_.CreateUniqueTempDir());
  }
  ~OnDeviceTranslationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  const base::FilePath& GetTempDir() { return tmp_dir_.GetPath(); }

 private:
  base::ScopedTempDir tmp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the behavior of createTranslator().
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest, SimpleTranslation) {
  MockComponentManager mock_component_manager(GetTempDir());
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

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
  ASSERT_EQ(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   R"(
  (() => {
    try {
      window._testPromise = translation.createTranslator({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
      return 'OK';
    } catch (e) {
      return e;
    }
    })();
  )")
                .ExtractString(),
            "OK");

  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called.
  run_loop_for_register_language_pack.Run();
  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();
  // Install the mock language pack.
  mock_component_manager.InstallMockLanguagePack(
      LanguagePackKey::kEn_Ja, CreateFakeDictionaryData("en", "ja"));
  // Translate "hello" to Japanese.
  // Note: the mock TranslateKit component returns the concatenation of the
  // content of "dict.dat" in the language pack and the input text.
  // See comments in mock_translate_kit_lib.cc for more details.
  EXPECT_EQ(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   R"(
        (async () => {
          try {
            const translator = await window._testPromise;
            return await translator.translate('hello');
          } catch (e) {
            return e;
          }
        })();
      )")
                .ExtractString(),
            "en to ja: hello");
}

// Tests the behavior of TranslationAPILimitLanguagePackCount
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       ExceedLanguagePackCount) {
  MockComponentManager mock_component_manager(GetTempDir());
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  EXPECT_CALL(mock_component_manager, RegisterTranslateKitComponentImpl())
      .Times(1);
  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();

  EXPECT_CALL(mock_component_manager,
              RegisterTranslateKitLanguagePackComponent(_))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Ja);
        mock_component_manager.InstallMockLanguagePackLater(
            key, CreateFakeDictionaryData("en", "ja"));
      }))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Es);
        mock_component_manager.InstallMockLanguagePackLater(
            key, CreateFakeDictionaryData("en", "es"));
      }))
      .WillOnce(Invoke([&](LanguagePackKey key) {
        EXPECT_EQ(key, LanguagePackKey::kEn_Zh);
        mock_component_manager.InstallMockLanguagePackLater(
            key, CreateFakeDictionaryData("en", "zh"));
      }));

  // Create a translator for En => Ja.
  TestSimpleTranslationWorks(browser(), "en", "ja");
  // Create a translator for En => Es.
  TestSimpleTranslationWorks(browser(), "en", "es");
  // Create a translator for En => Zh.
  TestSimpleTranslationWorks(browser(), "en", "zh");
  // Create a translator for En => Hi.
  TestCreateTranslator(browser(), "en", "hi",
                       "NotSupportedError: Unable to create translator for the "
                       "given source and target language.");
}

// Tests the behavior of createTranslator() when the target language is
// unsupported.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       CreateTranslatorUnsupportedLanguage) {
  MockComponentManager mock_component_manager(GetTempDir());
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  // Create a translator for unsupported language.
  TestCreateTranslator(browser(), "en", "xx",
                       "NotSupportedError: Unable to create translator for the "
                       "given source and target language.");
}

// Tests that the browser process can handle the case that the frame is deleted
// while creating a translator.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationBrowserTest,
                       FrameDeletedWhileCreatingATranslator) {
  MockComponentManager mock_component_manager(GetTempDir());
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

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

  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // Create a translator in an iframe.
  EXPECT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     R"(
  (() => {
      window._testIframe = document.createElement('iframe');
      document.body.appendChild(window._testIframe);
      window._testIframe.contentWindow.translation.createTranslator({
          sourceLanguage: 'en',
          targetLanguage: 'ja',
        });
    })();
  )"));
  // Wait until RegisterTranslateKitComponentImpl() is called.
  run_loop_for_register_translate_kit.Run();
  // Wait until RegisterTranslateKitLanguagePackComponent() is called.
  run_loop_for_register_language_pack.Run();

  // Deletes the iframe after the browser process receives the request.
  EXPECT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "document.body.removeChild(window._testIframe);"));

  // Install the mock TranslateKit component.
  mock_component_manager.InstallMockTranslateKitComponent();
  // Install the mock language pack.
  mock_component_manager.InstallMockLanguagePack(
      LanguagePackKey::kEn_Ja, CreateFakeDictionaryData("en", "ja"));

  // Test that Translator API works even after the frame requesting a translator
  // is deleted.
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

class OnDeviceTranslationCommandLineBrowserTest
    : public OnDeviceTranslationBrowserTest {
 public:
  OnDeviceTranslationCommandLineBrowserTest() = default;
  ~OnDeviceTranslationCommandLineBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchPath("translate-kit-binary-path",
                                   GetMockLibraryPath());

    const auto dict_dir_path = GetTempDir().AppendASCII("en_ja");
    const auto dict_path = dict_dir_path.AppendASCII("dict.dat");
    CHECK(base::CreateDirectory(dict_dir_path));
    CHECK(
        base::File(dict_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
            .WriteAndCheck(
                0, base::as_byte_span(CreateFakeDictionaryData("en", "ja"))));
    command_line->AppendSwitchASCII(
        "translate-kit-packages",
        base::StrCat({"en,ja,", dict_dir_path.AsUTF8Unsafe()}));
  }
};

// Tests the behavior of createTranslator() when the command line flags are
// provided.
IN_PROC_BROWSER_TEST_F(OnDeviceTranslationCommandLineBrowserTest,
                       SimpleTranslation) {
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  TestSimpleTranslationWorks(browser(), "en", "ja");
}

}  // namespace on_device_translation
