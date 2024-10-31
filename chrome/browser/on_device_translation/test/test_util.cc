// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/test/test_util.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/test/test_util.h"
#include "content/public/test/browser_test_utils.h"

namespace on_device_translation {

MockComponentManager::MockComponentManager(const base::FilePath& package_dir)
    : package_dir_(package_dir) {
  ComponentManager::SetForTesting(this);
}

MockComponentManager::~MockComponentManager() {
  ComponentManager::SetForTesting(nullptr);
}

void MockComponentManager::InstallMockTranslateKitComponent() {
  base::ScopedAllowBlockingForTesting allow_io;
  const auto mock_library_path = GetMockLibraryPath();
  const auto binary_path = package_dir_.Append(mock_library_path.BaseName());
  if (!base::DirectoryExists(binary_path.DirName())) {
    CHECK(base::CreateDirectory(binary_path.DirName()));
  }
  CHECK(base::CopyFile(mock_library_path, binary_path));
  g_browser_process->local_state()->SetFilePath(prefs::kTranslateKitBinaryPath,
                                                binary_path);
}

void MockComponentManager::InstallMockLanguagePack(
    LanguagePackKey language_pack_key,
    const std::string& fake_dictionary_data) {
  base::ScopedAllowBlockingForTesting allow_io;
  const auto dict_dir_path =
      package_dir_.AppendASCII(GetPackageInstallDirName(language_pack_key));
  const auto dict_path = dict_dir_path.AppendASCII("dict.dat");
  if (!base::DirectoryExists(dict_dir_path)) {
    CHECK(base::CreateDirectory(dict_dir_path));
  }
  CHECK(base::File(dict_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
            .WriteAndCheck(0, base::as_byte_span(fake_dictionary_data)));
  const LanguagePackComponentConfig* config =
      kLanguagePackComponentConfigMap.at(language_pack_key);
  g_browser_process->local_state()->SetBoolean(
      GetRegisteredFlagPrefName(*config), true);
  g_browser_process->local_state()->SetFilePath(
      GetComponentPathPrefName(*config), dict_dir_path);
}

void MockComponentManager::InstallMockLanguagePackLater(
    LanguagePackKey language_pack_key,
    const std::string_view fake_dictionary_data) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockComponentManager::InstallMockLanguagePack,
                     weak_ptr_factory_.GetWeakPtr(), language_pack_key,
                     std::string(fake_dictionary_data)));
}

std::string CreateFakeDictionaryData(const std::string& sourceLang,
                                     const std::string& targetLang) {
  return base::StringPrintf("%s to %s: ", sourceLang, targetLang);
}

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

// Tests that the createTranslator() returns the expected result.
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

}  // namespace on_device_translation
