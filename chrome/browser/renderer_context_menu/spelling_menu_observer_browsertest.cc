// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "content/public/common/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A test class used in this file. This test should be a browser test because it
// accesses resources.
class SpellingMenuObserverTest : public InProcessBrowserTest {
 public:
  SpellingMenuObserverTest();

  void SetUpOnMainThread() override { Reset(false); }

  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    observer_.reset();
    menu_.reset(new MockRenderViewContextMenu(incognito));
    observer_.reset(new SpellingMenuObserver(menu_.get()));
    menu_->SetObserver(observer_.get());
  }

  void InitMenu(const char* word, const char* suggestion) {
    content::ContextMenuParams params;
    params.is_editable = true;
    params.misspelled_word = base::ASCIIToUTF16(word);
    params.dictionary_suggestions.clear();
    if (suggestion)
      params.dictionary_suggestions.push_back(base::ASCIIToUTF16(suggestion));
    observer_->InitMenu(params);
  }

  void ForceSuggestMode() {
    menu()->GetPrefs()->SetBoolean(
        spellcheck::prefs::kSpellCheckUseSpellingService, true);
    // Force a non-empty and non-"en" locale so SUGGEST is available.
    base::ListValue dictionary;
    dictionary.AppendString("fr");
    menu()->GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                            dictionary);

    ASSERT_TRUE(SpellingServiceClient::IsAvailable(
        menu()->GetBrowserContext(), SpellingServiceClient::SUGGEST));
    ASSERT_FALSE(SpellingServiceClient::IsAvailable(
        menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));
  }

  ~SpellingMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  SpellingMenuObserver* observer() { return observer_.get(); }
 private:
  std::unique_ptr<SpellingMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
  DISALLOW_COPY_AND_ASSIGN(SpellingMenuObserverTest);
};

SpellingMenuObserverTest::SpellingMenuObserverTest() {
}

SpellingMenuObserverTest::~SpellingMenuObserverTest() {
}

}  // namespace

// Tests that right-clicking a correct word does not add any items.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithCorrectWord) {
  InitMenu("", nullptr);
  EXPECT_EQ(static_cast<size_t>(0), menu()->GetMenuSize());
}

// Tests that right-clicking a misspelled word adds two items:
// "Add to dictionary", "Use enhanced spell check".
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithMisspelledWord) {
  InitMenu("wiimode", nullptr);
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

// Tests that right-clicking a correct word when we enable spelling-service
// integration to verify an item "Use enhanced spell check" is checked. Even
// though this meanu itself does not add this item, its sub-menu adds the item
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
  base::ListValue dictionary;
  menu()->GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                          dictionary);

  InitMenu("wiimode", nullptr);
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
  base::ListValue dictionary;
  dictionary.AppendString("en");
  menu()->GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                          dictionary);

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
#if defined(OS_WIN)
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
  base::ListValue dictionary;
  dictionary.AppendString("en");
  menu()->GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                          dictionary);

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
#if defined(OS_WIN)
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
