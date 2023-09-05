// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/common/extensions/api/language_settings_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/event_router.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/ime/ash/input_method_manager.h"
#endif

namespace content {
class BrowserContext;
}

namespace extensions {

// Observes language settings and routes changes as events to listeners of the
// languageSettingsPrivate API.
class LanguageSettingsPrivateDelegate
    : public KeyedService,
      public EventRouter::Observer,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      public ash::input_method::InputMethodManager::Observer,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      public SpellcheckHunspellDictionary::Observer,
      public SpellcheckCustomDictionary::Observer {
 public:
  static std::unique_ptr<LanguageSettingsPrivateDelegate> Create(
      content::BrowserContext* browser_context);

  explicit LanguageSettingsPrivateDelegate(content::BrowserContext* context);
  LanguageSettingsPrivateDelegate(const LanguageSettingsPrivateDelegate&) =
      delete;
  LanguageSettingsPrivateDelegate& operator=(
      const LanguageSettingsPrivateDelegate&) = delete;

  ~LanguageSettingsPrivateDelegate() override;

  // Returns the languages and statuses of the enabled spellcheck dictionaries.
  virtual std::vector<
      api::language_settings_private::SpellcheckDictionaryStatus>
  GetHunspellDictionaryStatuses();

  // Retry downloading the spellcheck dictionary.
  virtual void RetryDownloadHunspellDictionary(const std::string& language);

 protected:
  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ash::input_method::InputMethodManager::Observer implementation.
  void InputMethodChanged(ash::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;
  void OnInputMethodExtensionAdded(const std::string& extension_id) override;
  void OnInputMethodExtensionRemoved(const std::string& extension_id) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // SpellcheckHunspellDictionary::Observer implementation.
  void OnHunspellDictionaryInitialized(const std::string& language) override;
  void OnHunspellDictionaryDownloadBegin(const std::string& language) override;
  void OnHunspellDictionaryDownloadSuccess(
      const std::string& language) override;
  void OnHunspellDictionaryDownloadFailure(
      const std::string& language) override;

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) override;

 private:
  typedef std::vector<base::WeakPtr<SpellcheckHunspellDictionary>>
      WeakDictionaries;

  // Updates the dictionaries that are used for spellchecking.
  void RefreshDictionaries(bool was_listening, bool should_listen);

  // Returns the hunspell dictionaries that are used for spellchecking.
  const WeakDictionaries& GetHunspellDictionaries();

  // If there are any JavaScript listeners registered for spellcheck events,
  // ensures we are registered for change notifications. Otherwise, unregisters
  // any observers.
  void StartOrStopListeningForSpellcheckChanges();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If there are any JavaScript listeners registered for input method events,
  // ensures we are registered for change notifications. Otherwise, unregisters
  // any observers.
  void StartOrStopListeningForInputMethodChanges();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Handles the preference for which languages should be used for spellcheck
  // by resetting the dictionaries and broadcasting an event.
  void OnSpellcheckDictionariesChanged();

  // Broadcasts an event with the list of spellcheck dictionary statuses.
  void BroadcastDictionariesChangedEvent();

  // Removes observers from hunspell_dictionaries_.
  void RemoveDictionaryObservers();

  // The hunspell dictionaries that are used for spellchecking.
  // TODO(aee): Consider replacing with
  // |SpellcheckService::GetHunspellDictionaries()|.
  WeakDictionaries hunspell_dictionaries_;

  // The custom dictionary that is used for spellchecking.
  raw_ptr<SpellcheckCustomDictionary> custom_dictionary_;

  raw_ptr<content::BrowserContext> context_;

  // True if there are observers listening for spellcheck events.
  bool listening_spellcheck_;
  // True if there are observers listening for input method events.
  bool listening_input_method_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_H_
