// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "testing/gmock/include/gmock/gmock.h"

class Browser;

namespace on_device_translation {

class MockComponentManager : public ComponentManager {
 public:
  explicit MockComponentManager(const base::FilePath& package_dir);
  ~MockComponentManager() override;

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
  void InstallMockTranslateKitComponent();

  // Installs the mock language pack.
  void InstallMockLanguagePack(LanguagePackKey language_pack_key,
                               const std::string& fake_dictionary_data);

  // Post a task to call InstallMockLanguagePack()
  void InstallMockLanguagePackLater(
      LanguagePackKey language_pack_key,
      const std::string_view fake_dictionary_data);

 private:
  const base::FilePath package_dir_;
  base::WeakPtrFactory<MockComponentManager> weak_ptr_factory_{this};
};

// Creates a fake dictionary data file for the given source and target
// languages.
// The content of the file is in the format of:
//   <sourceLang> to <targetLang>: <translation>
// For example, "en to ja: hello".
std::string CreateFakeDictionaryData(const std::string& sourceLang,
                                     const std::string& targetLang);

// Tests that the simple translation works. The dictionary data generated using
// CreateFakeDictionaryData() must be installed to pass the test.
void TestSimpleTranslationWorks(Browser* browser,
                                const std::string& sourceLang,
                                const std::string& targetLang);

// Tests that the createTranslator() returns the expected result.
void TestCreateTranslator(Browser* browser,
                          const std::string& sourceLang,
                          const std::string& targetLang,
                          const std::string& result);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_
