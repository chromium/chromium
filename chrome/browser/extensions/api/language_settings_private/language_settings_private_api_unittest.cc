// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/fake_input_method_delegate.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#endif

namespace extensions {

typedef api::language_settings_private::SpellcheckDictionaryStatus
    DictionaryStatus;

class MockLanguageSettingsPrivateDelegate
    : public LanguageSettingsPrivateDelegate {
 public:
  explicit MockLanguageSettingsPrivateDelegate(content::BrowserContext* context)
      : LanguageSettingsPrivateDelegate(context) {}
  ~MockLanguageSettingsPrivateDelegate() override {}

  // LanguageSettingsPrivateDelegate:
  std::vector<DictionaryStatus> GetHunspellDictionaryStatuses() override;
  void RetryDownloadHunspellDictionary(const std::string& language) override;

  std::vector<std::string> retry_download_hunspell_dictionary_called_with() {
    return retry_download_hunspell_dictionary_called_with_;
  }

 private:
  std::vector<std::string> retry_download_hunspell_dictionary_called_with_;
};

std::vector<DictionaryStatus>
MockLanguageSettingsPrivateDelegate::GetHunspellDictionaryStatuses() {
  std::vector<DictionaryStatus> statuses;
  DictionaryStatus status;
  status.language_code = "fr";
  status.is_ready = false;
  status.is_downloading = std::make_unique<bool>(true);
  status.download_failed = std::make_unique<bool>(false);
  statuses.push_back(std::move(status));
  return statuses;
}

void MockLanguageSettingsPrivateDelegate::RetryDownloadHunspellDictionary(
    const std::string& language) {
  retry_download_hunspell_dictionary_called_with_.push_back(language);
}

namespace {

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, ExtensionPrefs::Get(profile));
}

std::unique_ptr<KeyedService> BuildLanguageSettingsPrivateDelegate(
    content::BrowserContext* profile) {
  return std::make_unique<MockLanguageSettingsPrivateDelegate>(profile);
}

}  // namespace

class LanguageSettingsPrivateApiTest : public ExtensionServiceTestBase {
 public:
  LanguageSettingsPrivateApiTest() = default;
  ~LanguageSettingsPrivateApiTest() override = default;

 private:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::InitializeEmptyExtensionService();
    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildEventRouter));

    LanguageSettingsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildLanguageSettingsPrivateDelegate));
  }

  void TearDown() override { ExtensionServiceTestBase::TearDown(); }

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(LanguageSettingsPrivateApiTest, RetryDownloadHunspellDictionaryTest) {
  MockLanguageSettingsPrivateDelegate* mock_delegate =
      static_cast<MockLanguageSettingsPrivateDelegate*>(
          LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
              browser_context()));

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateRetryDownloadDictionaryFunction>();

  EXPECT_EQ(
      0u,
      mock_delegate->retry_download_hunspell_dictionary_called_with().size());
  EXPECT_TRUE(
      api_test_utils::RunFunction(function.get(), "[\"fr\"]", profile()))
      << function->GetError();
  EXPECT_EQ(
      1u,
      mock_delegate->retry_download_hunspell_dictionary_called_with().size());
  EXPECT_EQ(
      "fr",
      mock_delegate->retry_download_hunspell_dictionary_called_with().front());
}

TEST_F(LanguageSettingsPrivateApiTest, GetSpellcheckDictionaryStatusesTest) {
  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction>();

  std::unique_ptr<base::Value> actual =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());
  EXPECT_TRUE(actual) << function->GetError();

  base::ListValue expected;
  auto expected_status = std::make_unique<base::DictionaryValue>();
  expected_status->SetString("languageCode", "fr");
  expected_status->SetBoolean("isReady", false);
  expected_status->SetBoolean("isDownloading", true);
  expected_status->SetBoolean("downloadFailed", false);
  expected.Append(std::move(expected_status));
  EXPECT_EQ(expected, *actual);
}

#if defined(OS_CHROMEOS)
namespace {

namespace input_method = chromeos::input_method;
using input_method::InputMethodDescriptor;
using input_method::InputMethodManager;

std::string GetExtensionImeId() {
  std::string kExtensionImeId = chromeos::extension_ime_util::GetInputMethodID(
      crx_file::id_util::GenerateId("test.extension.ime"), "us");
  return kExtensionImeId;
}

std::string GetComponentExtensionImeId() {
  std::string kComponentExtensionImeId =
      chromeos::extension_ime_util::GetComponentInputMethodID(
          crx_file::id_util::GenerateId("test.component.extension.ime"), "us");
  return kComponentExtensionImeId;
}

std::string GetArcImeId() {
  std::string kArcImeId = chromeos::extension_ime_util::GetArcInputMethodID(
      crx_file::id_util::GenerateId("test.arc.ime"), "us");
  return kArcImeId;
}

class TestInputMethodManager : public input_method::MockInputMethodManager {
 public:
  class TestState : public input_method::MockInputMethodManager::State {
   public:
    TestState() {
      // Set up three IMEs
      std::vector<std::string> layouts({"us"});
      std::vector<std::string> languages({"en-US"});
      std::vector<std::string> arc_languages(
          {chromeos::extension_ime_util::kArcImeLanguage});
      InputMethodDescriptor extension_ime(
          GetExtensionImeId(), "", "", layouts, languages,
          false /* is_login_keyboard */, GURL(), GURL());
      InputMethodDescriptor component_extension_ime(
          GetComponentExtensionImeId(), "", "", layouts, languages,
          false /* is_login_keyboard */, GURL(), GURL());
      InputMethodDescriptor arc_ime(
          GetArcImeId(), "", "", layouts, arc_languages,
          false /* is_login_keyboard */, GURL(), GURL());
      input_methods_ = {extension_ime, component_extension_ime, arc_ime};
    }

    void GetInputMethodExtensions(
        input_method::InputMethodDescriptors* descriptors) override {
      for (const auto& descriptor : input_methods_)
        descriptors->push_back(descriptor);
    }

    input_method::InputMethodDescriptors input_methods_;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override = default;

    DISALLOW_COPY_AND_ASSIGN(TestState);
  };

  TestInputMethodManager() : state_(new TestState), util_(&delegate_) {
    util_.AppendInputMethods(state_->input_methods_);
  }

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  input_method::InputMethodUtil* GetInputMethodUtil() override {
    return &util_;
  }

 private:
  scoped_refptr<TestState> state_;
  input_method::FakeInputMethodDelegate delegate_;
  input_method::InputMethodUtil util_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

}  // namespace

TEST_F(LanguageSettingsPrivateApiTest, AddInputMethodTest) {
  TestInputMethodManager::Initialize(new TestInputMethodManager);

  // Initialize relevant prefs.
  profile()->GetPrefs()->SetString(prefs::kLanguagePreferredLanguages, "en-US");
  StringPrefMember enabled_imes;
  enabled_imes.Init(prefs::kLanguageEnabledImes, profile()->GetPrefs());
  StringPrefMember preload_engines;
  preload_engines.Init(prefs::kLanguagePreloadEngines, profile()->GetPrefs());
  enabled_imes.SetValue(std::string());
  preload_engines.SetValue(std::string());

  {
    // Add an extension IME. kLanguageEnabledImes should be updated.
    auto function =
        base::MakeRefCounted<LanguageSettingsPrivateAddInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetExtensionImeId() + "\"]", profile());

    EXPECT_EQ(GetExtensionImeId(), enabled_imes.GetValue());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  enabled_imes.SetValue(std::string());
  preload_engines.SetValue(std::string());
  {
    // Add a component extension IME. kLanguagePreloadEngines should be
    // updated.
    auto function =
        base::MakeRefCounted<LanguageSettingsPrivateAddInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetComponentExtensionImeId() + "\"]",
        profile());

    EXPECT_TRUE(enabled_imes.GetValue().empty());
    EXPECT_EQ(GetComponentExtensionImeId(), preload_engines.GetValue());
  }

  enabled_imes.SetValue(std::string());
  preload_engines.SetValue(std::string());
  {
    // Add an ARC IME. kLanguageEnabledImes should be updated.
    auto function =
        base::MakeRefCounted<LanguageSettingsPrivateAddInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetArcImeId() + "\"]", profile());

    EXPECT_EQ(GetArcImeId(), enabled_imes.GetValue());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  TestInputMethodManager::Shutdown();
}

TEST_F(LanguageSettingsPrivateApiTest, RemoveInputMethodTest) {
  TestInputMethodManager::Initialize(new TestInputMethodManager);

  // Initialize relevant prefs.
  StringPrefMember enabled_imes;
  enabled_imes.Init(prefs::kLanguageEnabledImes, profile()->GetPrefs());
  StringPrefMember preload_engines;
  preload_engines.Init(prefs::kLanguagePreloadEngines, profile()->GetPrefs());

  enabled_imes.SetValue(
      base::JoinString({GetExtensionImeId(), GetArcImeId()}, ","));
  preload_engines.SetValue(GetComponentExtensionImeId());
  {
    // Remove an extension IME.
    auto function = base::MakeRefCounted<
        LanguageSettingsPrivateRemoveInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetExtensionImeId() + "\"]", profile());

    EXPECT_EQ(GetArcImeId(), enabled_imes.GetValue());
    EXPECT_EQ(GetComponentExtensionImeId(), preload_engines.GetValue());
  }

  {
    // Remove a component extension IME.
    auto function = base::MakeRefCounted<
        LanguageSettingsPrivateRemoveInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetComponentExtensionImeId() + "\"]",
        profile());

    EXPECT_EQ(GetArcImeId(), enabled_imes.GetValue());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  {
    // Remove an ARC IME.
    auto function = base::MakeRefCounted<
        LanguageSettingsPrivateRemoveInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetArcImeId() + "\"]", profile());

    EXPECT_TRUE(enabled_imes.GetValue().empty());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  TestInputMethodManager::Shutdown();
}

#endif  // OS_CHROMEOS

}  // namespace extensions
