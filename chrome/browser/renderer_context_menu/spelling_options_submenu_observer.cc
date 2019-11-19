// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_options_submenu_observer.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_separator_types.h"

using content::BrowserThread;

SpellingOptionsSubMenuObserver::SpellingOptionsSubMenuObserver(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel::Delegate* delegate,
    int group_id)
    : proxy_(proxy),
      submenu_model_(delegate),
      language_group_id_(group_id),
      num_selected_dictionaries_(0) {
  if (proxy_ && proxy_->GetBrowserContext()) {
    Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
    use_spelling_service_.Init(spellcheck::prefs::kSpellCheckUseSpellingService,
                               profile->GetPrefs());
  }
  DCHECK(proxy_);
}

SpellingOptionsSubMenuObserver::~SpellingOptionsSubMenuObserver() {}

void SpellingOptionsSubMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Add available spell-checker languages to the sub menu.
  content::BrowserContext* browser_context = proxy_->GetBrowserContext();
  DCHECK(browser_context);
  SpellcheckService::GetDictionaries(browser_context, &dictionaries_);
  DCHECK(dictionaries_.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::string app_locale = g_browser_process->GetApplicationLocale();

  if (dictionaries_.size() > 1) {
    submenu_model_.AddRadioItemWithStringId(
        IDC_SPELLCHECK_MULTI_LINGUAL,
        IDS_CONTENT_CONTEXT_SPELLCHECK_MULTI_LINGUAL, language_group_id_);
  }

  const size_t kMaxLanguages = static_cast<size_t>(
      IDC_SPELLCHECK_LANGUAGES_FIRST - IDC_SPELLCHECK_LANGUAGES_LAST);
  for (size_t i = 0; i < dictionaries_.size() && i < kMaxLanguages; ++i) {
    submenu_model_.AddRadioItem(
        IDC_SPELLCHECK_LANGUAGES_FIRST + i,
        l10n_util::GetDisplayNameForLocale(dictionaries_[i].language,
                                           app_locale, true),
        language_group_id_);
    if (dictionaries_[i].used_for_spellcheck)
      ++num_selected_dictionaries_;
  }

  // Add an item that opens the 'Settings - Languages' page. This item is
  // handled in RenderViewContextMenu.
  submenu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS,
                                     IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);
  submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  if (dictionaries_.size() > 0) {
    // Add a 'Use basic spell check' item in the sub menu.
    submenu_model_.AddCheckItem(
        IDC_CHECK_SPELLING_WHILE_TYPING,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_CHECK_SPELLING_WHILE_TYPING));

    // Add a check item 'Use enhanced spell check'. This item is handled in
    // SpellingMenuObserver.
    Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
    RenderViewContextMenu::AddSpellCheckServiceItem(
        &submenu_model_,
        profile->GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable) &&
            use_spelling_service_.GetValue());
  }

  proxy_->AddSubMenu(
      IDC_SPELLCHECK_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU),
      &submenu_model_);
}

bool SpellingOptionsSubMenuObserver::IsCommandIdSupported(int command_id) {
  // Allow Spell Check language items on sub menu for text area context menu.
  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    DCHECK_GT(IDC_SPELLCHECK_LANGUAGES_FIRST + dictionaries_.size(),
              static_cast<size_t>(command_id));
    return true;
  }

  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING:
    case IDC_SPELLCHECK_MENU:
    case IDC_SPELLCHECK_MULTI_LINGUAL:
      return true;
  }

  return false;
}

bool SpellingOptionsSubMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id == IDC_SPELLCHECK_MULTI_LINGUAL)
    return num_selected_dictionaries_ == dictionaries_.size();

  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    if (num_selected_dictionaries_ == dictionaries_.size())
      return dictionaries_.size() == 1;

    size_t dictionary_index =
        static_cast<size_t>(command_id - IDC_SPELLCHECK_LANGUAGES_FIRST);
    return dictionaries_[dictionary_index].used_for_spellcheck;
  }

  // Check box for 'Use basic spell check'.
  if (command_id == IDC_CHECK_SPELLING_WHILE_TYPING) {
    Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
    return profile->GetPrefs()->GetBoolean(
               spellcheck::prefs::kSpellCheckEnable) &&
           !profile->GetPrefs()->GetBoolean(
               spellcheck::prefs::kSpellCheckUseSpellingService);
  }

  return false;
}

bool SpellingOptionsSubMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile);
  const PrefService* pref = profile->GetPrefs();
  if ((command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
       command_id < IDC_SPELLCHECK_LANGUAGES_LAST) ||
      command_id == IDC_SPELLCHECK_MULTI_LINGUAL) {
    return pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
  }

  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING:
      return !pref->FindPreference(spellcheck::prefs::kSpellCheckEnable)
                  ->IsManaged();

    case IDC_SPELLCHECK_MENU:
      return true;
  }

  return false;
}

void SpellingOptionsSubMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  // Check to see if one of the spell check language ids have been clicked.
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile);

  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      static_cast<size_t>(command_id) <
          IDC_SPELLCHECK_LANGUAGES_FIRST + dictionaries_.size()) {
    size_t dictionary_index =
        static_cast<size_t>(command_id - IDC_SPELLCHECK_LANGUAGES_FIRST);
    StringListPrefMember dictionaries_pref;
    dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries,
                           profile->GetPrefs());
    dictionaries_pref.SetValue({dictionaries_[dictionary_index].language});
    return;
  }

  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING: {
      bool spellCheckEnabled =
          profile->GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
      bool enhancedSpellCheckEnabled = profile->GetPrefs()->GetBoolean(
          spellcheck::prefs::kSpellCheckUseSpellingService);

      if (spellCheckEnabled && !enhancedSpellCheckEnabled) {
        // User is turning off spell check
        profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                                        false);
      } else if (enhancedSpellCheckEnabled) {
        // Use is choosing 'basic' over 'enhanced'
        profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                                        true);
        profile->GetPrefs()->SetBoolean(
            spellcheck::prefs::kSpellCheckUseSpellingService, false);
      } else {
        // User is turning on spell check
        profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                                        true);
      }
      break;
    }

    case IDC_SPELLCHECK_MULTI_LINGUAL: {
      StringListPrefMember dictionaries_pref;
      dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries,
                             profile->GetPrefs());
      std::vector<std::string> all_languages;
      for (const auto& dictionary : dictionaries_)
        all_languages.push_back(dictionary.language);
      dictionaries_pref.SetValue(all_languages);
      break;
    }
  }
}
