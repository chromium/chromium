// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, ExtensionPrefs::Get(profile));
}

std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(profile);
}

}  // namespace

class LanguageSettingsPrivateDelegateTest
    : public ExtensionServiceTestBase,
      public SpellcheckHunspellDictionary::Observer {
 public:
  LanguageSettingsPrivateDelegateTest() = default;
  ~LanguageSettingsPrivateDelegateTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::InitializeEmptyExtensionService();
    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildEventRouter));

#if BUILDFLAG(IS_WIN)
    // Tests were designed assuming Hunspell dictionary used and may fail when
    // Windows spellcheck is enabled by default.
    spellcheck::ScopedDisableBrowserSpellCheckerForTesting
        disable_browser_spell_checker;
#endif  // BUILDFLAG(IS_WIN)

    base::Value::List language_codes;
    language_codes.Append("fr");
    profile()->GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                               base::Value(std::move(language_codes)));

    SpellcheckServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildSpellcheckService));

    // Wait until dictionary file is loaded.
    SpellcheckService* service =
        SpellcheckServiceFactory::GetForContext(profile());
    auto* dictionary = service->GetHunspellDictionaries().front().get();
    dictionary->AddObserver(this);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
    dictionary->RemoveObserver(this);

    delegate_ = LanguageSettingsPrivateDelegate::Create(browser_context());
  }

  void TearDown() override {
    delegate_.reset();
    ExtensionServiceTestBase::TearDown();
  }

  LanguageSettingsPrivateDelegate* delegate() { return delegate_.get(); }

 private:
  // SpellcheckHunspellDictionary::Observer implementation.
  void OnHunspellDictionaryInitialized(const std::string& language) override {}
  void OnHunspellDictionaryDownloadBegin(const std::string& language) override {
  }
  void OnHunspellDictionaryDownloadSuccess(
      const std::string& language) override {}
  void OnHunspellDictionaryDownloadFailure(
      const std::string& language) override {
    if (run_loop_)
      run_loop_->Quit();
  }

#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list_;
#endif  // BUILDFLAG(IS_WIN)
  std::unique_ptr<LanguageSettingsPrivateDelegate> delegate_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)

TEST_F(LanguageSettingsPrivateDelegateTest,
       RetryDownloadHunspellDictionaryTest) {
  auto beforeStatuses = delegate()->GetHunspellDictionaryStatuses();
  ASSERT_EQ(1u, beforeStatuses.size());
  ASSERT_EQ("fr", beforeStatuses.front().language_code);
  ASSERT_FALSE(beforeStatuses.front().is_ready);
  ASSERT_FALSE(beforeStatuses.front().is_downloading);
  ASSERT_TRUE(beforeStatuses.front().download_failed &&
              *beforeStatuses.front().download_failed);

  delegate()->RetryDownloadHunspellDictionary("fr");

  auto afterStatuses = delegate()->GetHunspellDictionaryStatuses();
  ASSERT_EQ(1u, afterStatuses.size());
  ASSERT_EQ("fr", afterStatuses.front().language_code);
  EXPECT_FALSE(afterStatuses.front().is_ready);
  EXPECT_TRUE(afterStatuses.front().is_downloading &&
              *afterStatuses.front().is_downloading);
  EXPECT_FALSE(afterStatuses.front().download_failed);
}

#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

}  // namespace extensions
