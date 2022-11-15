// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_options_submenu_observer.h"

#include <memory>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A test class used in this file. This test should be a browser test because it
// accesses resources.
class SpellingOptionsSubMenuObserverTest : public InProcessBrowserTest {
 public:
  SpellingOptionsSubMenuObserverTest() {}

  SpellingOptionsSubMenuObserverTest(
      const SpellingOptionsSubMenuObserverTest&) = delete;
  SpellingOptionsSubMenuObserverTest& operator=(
      const SpellingOptionsSubMenuObserverTest&) = delete;

  ~SpellingOptionsSubMenuObserverTest() override {}

  void SetUpOnMainThread() override {
    menu_ = std::make_unique<MockRenderViewContextMenu>(false);
    observer_ = std::make_unique<SpellingOptionsSubMenuObserver>(
        // Pass nullptr as a delegate so that submenu items do not get put into
        // MockRenderViewContextMenu::items_.
        menu_.get(), nullptr, 1);
    menu_->SetObserver(observer_.get());
  }

  void TearDownOnMainThread() override {
    menu_->SetObserver(nullptr);
    observer_.reset();
    menu_.reset();
  }

  void InitMenu(bool enable_spellcheck,
                bool use_spellchecking_service,
                const std::string& accept_languages,
                const std::vector<std::string>& dictionaries) {
    menu()->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                                   enable_spellcheck);
    menu()->GetPrefs()->SetBoolean(
        spellcheck::prefs::kSpellCheckUseSpellingService,
        use_spellchecking_service);
    menu()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                  accept_languages);
    base::Value::List dictionaries_value;
    for (const std::string& dict : dictionaries) {
      dictionaries_value.Append(dict);
    }
    menu()->GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                                std::move(dictionaries_value));
    observer()->InitMenu(content::ContextMenuParams());
  }

  void ExpectPreferences(bool spellcheck_enabled,
                         const std::vector<std::string>& dictionaries) {
    EXPECT_EQ(spellcheck_enabled, menu()->GetPrefs()->GetBoolean(
                                      spellcheck::prefs::kSpellCheckEnable));
    base::Value::List dictionaries_value;
    for (const std::string& dict : dictionaries) {
      dictionaries_value.Append(dict);
    }
    EXPECT_EQ(dictionaries_value,
              menu()->GetPrefs()->GetList(
                  spellcheck::prefs::kSpellCheckDictionaries));
  }

  MockRenderViewContextMenu* menu() { return menu_.get(); }
  SpellingOptionsSubMenuObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<MockRenderViewContextMenu> menu_;
  std::unique_ptr<SpellingOptionsSubMenuObserver> observer_;
};

// Tests that selecting the "Use basic spell check" item toggles the value
// of the "browser.enable_spellchecking" profile.
IN_PROC_BROWSER_TEST_F(SpellingOptionsSubMenuObserverTest, ToggleSpelling) {
  InitMenu(true, false, "en-US", std::vector<std::string>(1, "en-US"));

  // Verify this menu has the "Use basic spell check" item and this item
  // is checked.
  EXPECT_TRUE(menu()->IsCommandIdEnabled(IDC_CHECK_SPELLING_WHILE_TYPING));
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));
  EXPECT_FALSE(menu()->GetPrefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService));

  // Select this item and verify that the "Use basic spell check" item is
  // not checked. Also, verify that the value of "browser.enable_spellchecking"
  // is now false.
  menu()->ExecuteCommand(IDC_CHECK_SPELLING_WHILE_TYPING, 0);
  ExpectPreferences(false, std::vector<std::string>(1, "en-US"));
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));

  menu()->ExecuteCommand(IDC_CHECK_SPELLING_WHILE_TYPING, 1);
  ExpectPreferences(true, std::vector<std::string>(1, "en-US"));
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));
}

IN_PROC_BROWSER_TEST_F(SpellingOptionsSubMenuObserverTest,
                       ToggleSpellingWithEnhancedSpellCheck) {
  InitMenu(true, true, "en-US,es", std::vector<std::string>(1, "en-US"));

  // Verify that because 'Use Enhanced spell check' is checked, the
  // 'Use basic spell check' is shown as unchecked
  EXPECT_TRUE(menu()->IsCommandIdEnabled(IDC_CHECK_SPELLING_WHILE_TYPING));
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));

  // Select this item and verify that "Use basic spell check" item is now
  // checked and that 'spellcheck.use_spelling_service' is now disabled.
  menu()->ExecuteCommand(IDC_CHECK_SPELLING_WHILE_TYPING, 1);
  ExpectPreferences(true, std::vector<std::string>(1, "en-US"));
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));
  EXPECT_FALSE(menu()->GetPrefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService));
}

// Single accept language is selected based on the dictionaries preference.
// Consequently selecting multilingual spellcheck should copy all accept
// languages into spellcheck dictionaries preference.
IN_PROC_BROWSER_TEST_F(SpellingOptionsSubMenuObserverTest, SelectMultilingual) {
  InitMenu(true, false, "en-US,es", std::vector<std::string>(1, "en-US"));
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_MULTI_LINGUAL));
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_LANGUAGES_FIRST));
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_LANGUAGES_FIRST + 1));

  menu()->ExecuteCommand(IDC_SPELLCHECK_MULTI_LINGUAL, 0);
  std::vector<std::string> dictionaries;
  dictionaries.push_back("en-US");
  dictionaries.push_back("es");
  ExpectPreferences(true, dictionaries);
}

// Multilingual spellcheck is selected when all dictionaries are used for
// spellcheck. Consequently selecting "English (United States)" should set
// spellcheck dictionaries preferences to ["en-US"].
IN_PROC_BROWSER_TEST_F(SpellingOptionsSubMenuObserverTest,
                       SelectFirstLanguage) {
  std::vector<std::string> dictionaries;
  dictionaries.push_back("en-US");
  dictionaries.push_back("es");
  InitMenu(true, false, "en-US,es", dictionaries);
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_MULTI_LINGUAL));
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_LANGUAGES_FIRST));
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_LANGUAGES_FIRST + 1));

  menu()->ExecuteCommand(IDC_SPELLCHECK_LANGUAGES_FIRST, 0);
  ExpectPreferences(true, std::vector<std::string>(1, "en-US"));
}

// Single dictionary should be selected based on preferences.
IN_PROC_BROWSER_TEST_F(SpellingOptionsSubMenuObserverTest,
                       SingleLanguageSelected) {
  InitMenu(true, false, "en-US", std::vector<std::string>(1, "en-US"));
  EXPECT_TRUE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_LANGUAGES_FIRST));
}

// Single dictionary should be deselected based on preferences. Selecting the
// dictionary should update the preference.
IN_PROC_BROWSER_TEST_F(SpellingOptionsSubMenuObserverTest,
                       SelectTheOnlyLanguage) {
  InitMenu(true, false, "en-US", std::vector<std::string>());
  EXPECT_FALSE(menu()->IsCommandIdChecked(IDC_SPELLCHECK_LANGUAGES_FIRST));

  menu()->ExecuteCommand(IDC_SPELLCHECK_LANGUAGES_FIRST, 0);
  ExpectPreferences(true, std::vector<std::string>(1, "en-US"));
}

}  // namespace
