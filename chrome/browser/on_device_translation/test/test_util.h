// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/translation_manager_impl.h"
#include "content/public/browser/render_process_host.h"
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

  // Do not expect any call to RegisterTranslateKitComponent().
  void DoNotExpectCallRegisterTranslateKitComponent();

  // Expect one call to RegisterTranslateKitComponent() and install the mock
  // TranslateKit component later as a result of the call.
  void ExpectCallRegisterTranslateKitComponentAndInstall();

  // Do not expect any call to RegisterTranslateKitLanguagePackComponent().
  void DoNotExpectCallRegisterLanguagePackComponent();

  // Expect one call to RegisterTranslateKitLanguagePackComponent() for each
  // language pack key in `language_pack_keys` and install the fake language
  // pack later as a result of the call.
  void ExpectCallRegisterLanguagePackComponentAndInstall(
      const base::span<const LanguagePackKey>& language_pack_keys);

  // Installs the mock TranslateKit component.
  // The mock component's translate method returns the concatenation of the
  // content of "dict.dat" in the language pack and the input text.
  // See comments in mock_translate_kit_lib.cc for more details.
  void InstallMockTranslateKitComponent();

  // Installs the mock TranslateKit component which library does not contain any
  // of the methods in the TranslateKit API.
  void InstallMockInvalidFunctionPointerLibraryComponent();

  // Installs the mock TranslateKit component which library does contain all of
  // the methods in the TranslateKit API, but its CreateTranslateKit() method
  // always fails.
  void InstallMockFailingLibraryComponent();

  // Installs the mock TranslateKit component without any library.
  void InstallEmptyMockComponent();

  // Registers the mock language pack to the PrefService.
  void RegisterLanguagePack(LanguagePackKey language_pack_key);

  // Installs the mock language pack.
  void InstallMockLanguagePack(LanguagePackKey language_pack_key);
  void InstallMockLanguagePack(LanguagePackKey language_pack_key,
                               const std::string_view fake_dictionary_data);

  // Post a task to call InstallMockTranslateKitComponent()
  void InstallMockTranslateKitComponentLater();

  // Post a task to call InstallMockLanguagePack()
  void InstallMockLanguagePackLater(LanguagePackKey language_pack_key);

 private:
  // Installs the mock TranslateKit component with the given library path.
  void InstallComponent(base::FilePath library_path);

  const base::FilePath package_dir_;
  base::AutoReset<ComponentManager*> mock_component_manager_;
  base::WeakPtrFactory<MockComponentManager> weak_ptr_factory_{this};
};

class MockTranslationManagerImpl : public TranslationManagerImpl {
 public:
  explicit MockTranslationManagerImpl(content::RenderProcessHost* process_host,
                                      content::BrowserContext* browser_context,
                                      const url::Origin& origin);
  ~MockTranslationManagerImpl() override;

  MockTranslationManagerImpl(const MockTranslationManagerImpl&) = delete;
  MockTranslationManagerImpl& operator=(const MockTranslationManagerImpl&) =
      delete;

  MOCK_METHOD(base::TimeDelta, GetTranslatorDownloadDelay, (), (override));

  MOCK_METHOD(component_updater::ComponentUpdateService*,
              GetComponentUpdateService,
              (),
              (override));

  bool CrashesAllowed() override { return crashes_allowed_; }

  void SetCrashesAllowed(bool allow) { crashes_allowed_ = allow; }

 private:
  base::AutoReset<TranslationManagerImpl*> mock_translation_manager_impl_;
  bool crashes_allowed_ = false;
};

// Creates a fake dictionary data file for the given source and target
// languages.
// The content of the file is in the format of:
//   <sourceLang> to <targetLang>: <translation>
// For example, "en to ja: hello".
std::string CreateFakeDictionaryData(const std::string_view sourceLang,
                                     const std::string_view targetLang);

// Tests that the simple translation works. The dictionary data generated using
// CreateFakeDictionaryData() must be installed to pass the test.
void TestSimpleTranslationWorks(Browser* browser,
                                LanguagePackKey language_pack_key);
void TestSimpleTranslationWorks(Browser* browser,
                                const std::string_view sourceLang,
                                const std::string_view targetLang);

// Tests that the createTranslator() returns the expected result.
void TestCreateTranslator(Browser* browser,
                          LanguagePackKey language_pack_key,
                          const std::string_view result);
void TestCreateTranslator(Browser* browser,
                          const std::string_view sourceLang,
                          const std::string_view targetLang,
                          const std::string_view result);

// Tests that`availability()` returns the expected result.
void TestTranslationAvailable(Browser* browser,
                              const std::string_view sourceLang,
                              const std::string_view targetLang,
                              const std::string_view result);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_
