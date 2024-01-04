// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A test class used in this file. This test should be a browser test because it
// accesses resources.
class SpellingMenuObserverTest : public InProcessBrowserTest {
 public:
  SpellingMenuObserverTest();

  void SetUpOnMainThread() override {
    Reset(false);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    base::Value::List dictionary;
    dictionary.Append("en-US");
    menu()->GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                                std::move(dictionary));
    // Use SetTestingFactoryAndUse to force creation and initialization of
    // SpellcheckService using the TestingProfile browser context.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        menu()->GetBrowserContext(),
        base::BindRepeating(&SpellingMenuObserverTest::BuildSpellcheckService,
                            base::Unretained(this)));
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  }

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  std::unique_ptr<KeyedService> BuildSpellcheckService(
      content::BrowserContext* context) {
    auto spellcheck_service = std::make_unique<SpellcheckService>(context);

    // Call SetLanguage to assure that the platform spellchecker is initialized.
    spellcheck_platform::SetLanguage(
        spellcheck_service->platform_spell_checker(), "en-US",
        base::BindOnce(&SpellingMenuObserverTest::OnSetLanguageComplete,
                       base::Unretained(this)));

    RunUntilCallbackReceived();

    return spellcheck_service;
  }

  void OnSetLanguageComplete(bool result) {
    ASSERT_TRUE(result);
    callback_received_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  void OnSuggestionsComplete() {
    callback_received_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  void RunUntilCallbackReceived() {
    if (callback_received_)
      return;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    // Reset status.
    callback_received_ = false;
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    observer_.reset();
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = std::make_unique<SpellingMenuObserver>(menu_.get());
    menu_->SetObserver(observer_.get());
  }

  void InitMenu(const char* word, const char* suggestion) {
    content::ContextMenuParams params;
    params.is_editable = true;
    params.misspelled_word = base::ASCIIToUTF16(word);
    params.dictionary_suggestions.clear();
    if (suggestion)
      params.dictionary_suggestions.push_back(base::ASCIIToUTF16(suggestion));

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    // Expect early return if word is spelled correctly.
    if (params.misspelled_word.empty())
      callback_received_ = true;

    observer_->RegisterSuggestionsCompleteCallbackForTesting(
        base::BindOnce(&SpellingMenuObserverTest::OnSuggestionsComplete,
                       base::Unretained(this)));
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    observer_->InitMenu(params);

    // Windows behavior needs this to be called as well to update placeholder
    // menu items. Doesn't hurt for non-Windows platforms either.
    observer_->OnContextMenuShown(params, gfx::Rect());

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    RunUntilCallbackReceived();
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  }

  void ForceSuggestMode() {
    menu()->GetPrefs()->SetBoolean(
        spellcheck::prefs::kSpellCheckUseSpellingService, true);
    // Force a non-empty and non-"en" locale so SUGGEST is available.
    base::Value::List dictionary;
    dictionary.Append("fr");
    menu()->GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                                std::move(dictionary));

    ASSERT_TRUE(SpellingServiceClient::IsAvailable(
        menu()->GetBrowserContext(), SpellingServiceClient::SUGGEST));
    ASSERT_FALSE(SpellingServiceClient::IsAvailable(
        menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));
  }

  SpellingMenuObserverTest(const SpellingMenuObserverTest&) = delete;
  SpellingMenuObserverTest& operator=(const SpellingMenuObserverTest&) = delete;

  ~SpellingMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  SpellingMenuObserver* observer() { return observer_.get(); }
 private:
  std::unique_ptr<SpellingMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Quits the RunLoop on receiving callbacks.
  base::OnceClosure quit_;

  // Flag used for early exit from RunLoop if callback already received.
  bool callback_received_ = false;

  base::test::ScopedFeatureList feature_list_;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
};

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
SpellingMenuObserverTest::SpellingMenuObserverTest() {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{spellcheck::kWinRetrieveSuggestionsOnlyOnDemand},
      /*disabled_features=*/{spellcheck::kWinDelaySpellcheckServiceInit});
}
#else
SpellingMenuObserverTest::SpellingMenuObserverTest() = default;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

SpellingMenuObserverTest::~SpellingMenuObserverTest() = default;

}  // namespace

// Tests that right-clicking a correct word does not add any items.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithCorrectWord) {
  InitMenu("", nullptr);
  EXPECT_EQ(static_cast<size_t>(0), menu()->GetMenuSize());
}

// Tests that right-clicking a misspelled word adds two items:
// "Add to dictionary", "Use enhanced spell check".
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithMisspelledWord) {
  // Pick word that Windows platform spellcheck has no suggestions for.
  InitMenu("missssspelling", nullptr);
  EXPECT_EQ(2U, menu()->GetMenuSize());

  // Read all the context-menu items added by this test and verify they are
  // expected ones. We do not check the item titles to prevent resource changes
  // from breaking this test. (I think it is not expected by those who change
  // resources.)
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  menu()->GetMenuItem(2, &item);
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Tests that right-clicking a misspelled word that is identified as misspelled
// by both Hunspell and Windows platform combines their suggestions.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       WinInitMenuWithMisspelledWordCombined) {
  InitMenu("mispelled", "misspelling");
  EXPECT_EQ(6U, menu()->GetMenuSize());

  // Read all the context-menu items added by this test and verify they are
  // expected ones.
  MockRenderViewContextMenu::MockMenuItem item;
  // First separator.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // First suggestion.
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_EQ(u"misspelled", item.title);
  // Second suggestion.
  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0 + 1, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_EQ(u"misspelling", item.title);
  // Second separator.
  menu()->GetMenuItem(3, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // Add to dictionary.
  menu()->GetMenuItem(4, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // Enhanced spellcheck toggle.
  menu()->GetMenuItem(5, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
}

// Tests that right-clicking a misspelled word that is identified as misspelled
// by both Hunspell and Windows platform with the same suggestion leads to a
// single suggestion.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       WinInitMenuWithMisspelledWordNoDuplicateSuggestions) {
  InitMenu("mispelled", "misspelled");
  EXPECT_EQ(5U, menu()->GetMenuSize());

  // Read all the context-menu items added by this test and verify they are
  // expected ones.
  MockRenderViewContextMenu::MockMenuItem item;
  // First separator.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // First and only suggestion.
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_EQ(u"misspelled", item.title);
  // Second separator.
  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // Add to dictionary.
  menu()->GetMenuItem(3, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // Enhanced spellcheck toggle.
  menu()->GetMenuItem(4, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
}

// Tests that right-clicking a misspelled word that is identified as misspelled
// by both Hunspell and Windows platform that has > 3 suggestions only displays
// 3 suggestions.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       WinInitMenuWithMisspelledWordMaxSuggestions) {
  InitMenu("wtree", "wee");
  EXPECT_EQ(7U, menu()->GetMenuSize());

  std::set<std::u16string> suggestions(
      {u"tree", u"twee", u"wee", u"ware", u"were"});
  bool wee_suggested = false;
  for (unsigned int i = 1; i < menu()->GetMenuSize(); i++) {
    MockRenderViewContextMenu::MockMenuItem item;
    menu()->GetMenuItem(i, &item);
    if (!item.title.compare(u"wee")) {
      wee_suggested = true;
      break;
    }
  }
  EXPECT_TRUE(wee_suggested);
  // Read all the context-menu items added by this test and verify they are
  // among the expected possibilities.
  MockRenderViewContextMenu::MockMenuItem item;
  // First separator.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // First suggestion.
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_TRUE(suggestions.contains(item.title));
  // Second suggestion.
  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0 + 1, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_TRUE(suggestions.contains(item.title));
  // Third suggestion.
  menu()->GetMenuItem(3, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0 + 2, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_TRUE(suggestions.contains(item.title));
  // Second separator.
  menu()->GetMenuItem(4, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // Add to dictionary.
  menu()->GetMenuItem(5, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  // Enhanced spellcheck toggle.
  menu()->GetMenuItem(6, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

// Tests that right-clicking a correct word when we enable spelling-service
// integration to verify an item "Use enhanced spell check" is checked. Even
// though this menu itself does not add this item, its sub-menu adds the item
// and calls SpellingMenuObserver::IsChecked() to check it.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       EnableSpellingServiceWithCorrectWord) {
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);
  InitMenu("", nullptr);

  EXPECT_TRUE(
      observer()->IsCommandIdChecked(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE));
}

// Tests that right-clicking a misspelled word when we enable spelling-service
// integration to verify an item "Use enhanced spell check" is checked. (This
// test does not actually send JSON-RPC requests to the service because it makes
// this test flaky.)
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, EnableSpellingService) {
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);
  base::Value::List dictionary;
  menu()->GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                              std::move(dictionary));

  // Pick word that Windows platform spellcheck has no suggestions for.
  InitMenu("missssspelling", nullptr);
  EXPECT_EQ(2U, menu()->GetMenuSize());

  // To avoid duplicates, this test reads only the "Use enhanced spell check"
  // item and verifies it is enabled and checked.
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}

// Tests that right-clicking a misspelled word when spelling-service
// integration is enabled but the browser's spell check preference is disabled
// shows the "Use enhanced spell check" as unchecked. Clicking on this item
// enables spell check on the browser.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       EnableSpellingServiceWhenSpellcheckDisabled) {
  menu()->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);

  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(1, &item);
  EXPECT_FALSE(item.checked);

  menu()->ExecuteCommand(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, 1);
  EXPECT_TRUE(
      menu()->GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE));
}

// Test that we don't show "No more suggestions from Google" if the spelling
// service is enabled and that there is only one suggestion.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       NoMoreSuggestionsNotDisplayed) {
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);

  // Force a non-empty locale so SPELLCHECK is available.
  base::Value::List dictionary;
  dictionary.Append("en");
  menu()->GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                              std::move(dictionary));

  EXPECT_TRUE(SpellingServiceClient::IsAvailable(
      menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));
  InitMenu("asdfkj", "asdf");

  // The test should see a suggestion, separator, "Add to dictionary",
  // "Use enhanced spell check".
  // Possibly more items (not relevant here).
  EXPECT_LT(3U, menu()->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(3, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(4, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}

// crbug.com/899935
#if BUILDFLAG(IS_WIN)
#define MAYBE_NoSpellingServiceWhenOffTheRecord \
  DISABLED_NoSpellingServiceWhenOffTheRecord
#else
#define MAYBE_NoSpellingServiceWhenOffTheRecord \
  NoSpellingServiceWhenOffTheRecord
#endif

// Test that "Use enhanced spell check" is grayed out when using an
// off the record profile.
// TODO(rlp): Include graying out of autocorrect in this test when autocorrect
// is functional.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       MAYBE_NoSpellingServiceWhenOffTheRecord) {
  // Create a menu in an incognito profile.
  Reset(true);

  // This means spellchecking is allowed. Default is that the service is
  // contacted but this test makes sure that if profile is incognito, that
  // is not an option.
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);

  // Force a non-empty locale so SUGGEST normally would be available.
  base::Value::List dictionary;
  dictionary.Append("en");
  menu()->GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                              std::move(dictionary));

  EXPECT_FALSE(SpellingServiceClient::IsAvailable(
      menu()->GetBrowserContext(), SpellingServiceClient::SUGGEST));
  EXPECT_FALSE(SpellingServiceClient::IsAvailable(
      menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));

  InitMenu("sjxdjiiiiii", nullptr);

  // There should not be a "No more Google suggestions" (from SpellingService)
  // or a separator. The next 2 items should be "Add to Dictionary" followed
  // by "Use enhanced spell check" which should be disabled.
  // TODO(rlp): add autocorrect here when it is functional.
  EXPECT_LT(1U, menu()->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// crbug.com/899935
#if BUILDFLAG(IS_WIN)
#define MAYBE_SuggestionsForceTopSeparator DISABLED_SuggestionsForceTopSeparator
#else
#define MAYBE_SuggestionsForceTopSeparator SuggestionsForceTopSeparator
#endif

// Test that the menu is preceeded by a separator if there are any suggestions,
// or if the SpellingServiceClient is available
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       MAYBE_SuggestionsForceTopSeparator) {
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);

  // First case: Misspelled word, no suggestions, no spellcheck service.
  InitMenu("asdfkj", nullptr);
  // See SpellingMenuObserverTest.InitMenuWithMisspelledWord on why 2 items.
  EXPECT_EQ(2U, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_NE(-1, item.command_id);

  // Case #2. Misspelled word, suggestions, no spellcheck service.
  Reset(false);
  menu()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);
  InitMenu("asdfkj", "asdf");

  // Expect at least separator and 4 default entries.
  EXPECT_LT(4U, menu()->GetMenuSize());
  // This test only cares that the first one is a separator.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);

  // Case #3. Misspelled word, suggestion service is on.
  Reset(false);
  ForceSuggestMode();
  InitMenu("asdfkj", nullptr);

  // Should have at least 2 entries. Separator, suggestion.
  EXPECT_LT(2U, menu()->GetMenuSize());
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, item.command_id);
}
