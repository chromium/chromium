// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace extensions {

namespace language_settings_private = api::language_settings_private;

LanguageSettingsPrivateDelegate::LanguageSettingsPrivateDelegate(
    content::BrowserContext* context)
    : custom_dictionary_(nullptr),
      context_(context),
      listening_spellcheck_(false),
      listening_input_method_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router)
    return;

  event_router->RegisterObserver(this,
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName);
  event_router->RegisterObserver(this,
      language_settings_private::OnCustomDictionaryChanged::kEventName);
  event_router->RegisterObserver(
      this, language_settings_private::OnInputMethodAdded::kEventName);
  event_router->RegisterObserver(
      this, language_settings_private::OnInputMethodRemoved::kEventName);

  pref_change_registrar_.Init(
      Profile::FromBrowserContext(context_)->GetPrefs());

  StartOrStopListeningForSpellcheckChanges();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  StartOrStopListeningForInputMethodChanges();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

LanguageSettingsPrivateDelegate::~LanguageSettingsPrivateDelegate() {
  DCHECK(!listening_spellcheck_);
  DCHECK(!listening_input_method_);
  pref_change_registrar_.RemoveAll();
}

std::unique_ptr<LanguageSettingsPrivateDelegate>
LanguageSettingsPrivateDelegate::Create(content::BrowserContext* context) {
  return std::make_unique<LanguageSettingsPrivateDelegate>(context);
}

std::vector<language_settings_private::SpellcheckDictionaryStatus>
LanguageSettingsPrivateDelegate::GetHunspellDictionaryStatuses() {
  std::vector<language_settings_private::SpellcheckDictionaryStatus> statuses;
  for (const auto& dictionary : GetHunspellDictionaries()) {
    if (!dictionary)
      continue;
    language_settings_private::SpellcheckDictionaryStatus status;
    status.language_code = dictionary->GetLanguage();
    status.is_ready = dictionary->IsReady();
    if (!status.is_ready) {
      if (dictionary->IsDownloadInProgress())
        status.is_downloading = true;
      if (dictionary->IsDownloadFailure())
        status.download_failed = true;
    }
    statuses.push_back(std::move(status));
  }
  return statuses;
}

void LanguageSettingsPrivateDelegate::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_spellcheck_) {
    RemoveDictionaryObservers();
    listening_spellcheck_ = false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (listening_input_method_) {
    auto* input_method_manager = ash::input_method::InputMethodManager::Get();
    if (input_method_manager)
      input_method_manager->RemoveObserver(this);
    listening_input_method_ = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void LanguageSettingsPrivateDelegate::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to spellcheck change events.
  if (details.event_name ==
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName ||
      details.event_name ==
      language_settings_private::OnCustomDictionaryChanged::kEventName) {
    StartOrStopListeningForSpellcheckChanges();
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (details.event_name ==
          language_settings_private::OnInputMethodAdded::kEventName ||
      details.event_name ==
          language_settings_private::OnInputMethodRemoved::kEventName) {
    StartOrStopListeningForInputMethodChanges();
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void LanguageSettingsPrivateDelegate::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events if there are no more listeners.
  StartOrStopListeningForSpellcheckChanges();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  StartOrStopListeningForInputMethodChanges();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LanguageSettingsPrivateDelegate::InputMethodChanged(
    ash::input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  // Nothing to do.
}

void LanguageSettingsPrivateDelegate::OnInputMethodExtensionAdded(
    const std::string& extension_id) {
  auto args(
      language_settings_private::OnInputMethodAdded::Create(extension_id));
  std::unique_ptr<extensions::Event> extension_event(new extensions::Event(
      events::LANGUAGE_SETTINGS_PRIVATE_ON_INPUT_METHOD_ADDED,
      language_settings_private::OnInputMethodAdded::kEventName,
      std::move(args)));
  EventRouter::Get(context_)->BroadcastEvent(std::move(extension_event));
}

void LanguageSettingsPrivateDelegate::OnInputMethodExtensionRemoved(
    const std::string& extension_id) {
  auto args(
      language_settings_private::OnInputMethodRemoved::Create(extension_id));
  std::unique_ptr<extensions::Event> extension_event(new extensions::Event(
      events::LANGUAGE_SETTINGS_PRIVATE_ON_INPUT_METHOD_REMOVED,
      language_settings_private::OnInputMethodRemoved::kEventName,
      std::move(args)));
  EventRouter::Get(context_)->BroadcastEvent(std::move(extension_event));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryInitialized(
    const std::string& language) {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryDownloadBegin(
    const std::string& language) {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryDownloadSuccess(
    const std::string& language) {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryDownloadFailure(
    const std::string& language) {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnCustomDictionaryLoaded() {
}

void LanguageSettingsPrivateDelegate::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& change) {
  std::vector<std::string> to_add(change.to_add().begin(),
                                  change.to_add().end());
  std::vector<std::string> to_remove(change.to_remove().begin(),
                                     change.to_remove().end());
  auto args(language_settings_private::OnCustomDictionaryChanged::Create(
      to_add, to_remove));
  std::unique_ptr<Event> extension_event(new Event(
      events::LANGUAGE_SETTINGS_PRIVATE_ON_CUSTOM_DICTIONARY_CHANGED,
      language_settings_private::OnCustomDictionaryChanged::kEventName,
      std::move(args)));
  EventRouter::Get(context_)->BroadcastEvent(std::move(extension_event));
}

void LanguageSettingsPrivateDelegate::RefreshDictionaries(bool was_listening,
                                                          bool should_listen) {
  if (was_listening)
    RemoveDictionaryObservers();
  hunspell_dictionaries_.clear();
  SpellcheckService* service = SpellcheckServiceFactory::GetForContext(
      context_);
  if (!custom_dictionary_)
    custom_dictionary_ = service->GetCustomDictionary();

  const std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>&
      dictionaries(service->GetHunspellDictionaries());
  for (const auto& dictionary : dictionaries) {
    hunspell_dictionaries_.push_back(dictionary->AsWeakPtr());
    if (should_listen)
      dictionary->AddObserver(this);
  }
}

const LanguageSettingsPrivateDelegate::WeakDictionaries&
LanguageSettingsPrivateDelegate::GetHunspellDictionaries() {
  // If there are no hunspell dictionaries, or the first is invalid, refresh.
  if (hunspell_dictionaries_.empty() || !hunspell_dictionaries_.front())
    RefreshDictionaries(listening_spellcheck_, listening_spellcheck_);
  return hunspell_dictionaries_;
}

void LanguageSettingsPrivateDelegate::
    StartOrStopListeningForSpellcheckChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen =
      event_router->HasEventListener(language_settings_private::
          OnSpellcheckDictionariesChanged::kEventName) ||
      event_router->HasEventListener(language_settings_private::
          OnCustomDictionaryChanged::kEventName);

  if (should_listen && !listening_spellcheck_) {
    // Update and observe the hunspell dictionaries.
    RefreshDictionaries(listening_spellcheck_, should_listen);
    // Observe the dictionaries preference.
    pref_change_registrar_.Add(
        spellcheck::prefs::kSpellCheckDictionaries,
        base::BindRepeating(
            &LanguageSettingsPrivateDelegate::OnSpellcheckDictionariesChanged,
            base::Unretained(this)));
    // Observe the dictionary of custom words.
    if (custom_dictionary_)
      custom_dictionary_->AddObserver(this);
  } else if (!should_listen && listening_spellcheck_) {
    // Stop observing any dictionaries that still exist.
    RemoveDictionaryObservers();
    hunspell_dictionaries_.clear();
    pref_change_registrar_.Remove(spellcheck::prefs::kSpellCheckDictionaries);
    if (custom_dictionary_)
      custom_dictionary_->RemoveObserver(this);
  }

  listening_spellcheck_ = should_listen;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LanguageSettingsPrivateDelegate::
    StartOrStopListeningForInputMethodChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen =
      event_router->HasEventListener(
          language_settings_private::OnInputMethodAdded::kEventName) ||
      event_router->HasEventListener(
          language_settings_private::OnInputMethodRemoved::kEventName);

  auto* input_method_manager = ash::input_method::InputMethodManager::Get();
  if (input_method_manager) {
    if (should_listen && !listening_input_method_)
      input_method_manager->AddObserver(this);
    else if (!should_listen && listening_input_method_)
      input_method_manager->RemoveObserver(this);
  }

  listening_input_method_ = should_listen;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void LanguageSettingsPrivateDelegate::RetryDownloadHunspellDictionary(
    const std::string& language) {
  for (const base::WeakPtr<SpellcheckHunspellDictionary>& dictionary :
       GetHunspellDictionaries()) {
    if (dictionary && dictionary->GetLanguage() == language) {
      dictionary->RetryDownloadDictionary(context_);
      return;
    }
  }
}

void LanguageSettingsPrivateDelegate::OnSpellcheckDictionariesChanged() {
  RefreshDictionaries(listening_spellcheck_, listening_spellcheck_);
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::BroadcastDictionariesChangedEvent() {
  std::vector<language_settings_private::SpellcheckDictionaryStatus> statuses =
      GetHunspellDictionaryStatuses();

  auto args(language_settings_private::OnSpellcheckDictionariesChanged::Create(
      statuses));
  std::unique_ptr<extensions::Event> extension_event(new extensions::Event(
      events::LANGUAGE_SETTINGS_PRIVATE_ON_SPELLCHECK_DICTIONARIES_CHANGED,
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName,
      std::move(args)));
  EventRouter::Get(context_)->BroadcastEvent(std::move(extension_event));
}

void LanguageSettingsPrivateDelegate::RemoveDictionaryObservers() {
  for (const auto& dictionary : hunspell_dictionaries_) {
    if (dictionary)
      dictionary->RemoveObserver(this);
  }
}

}  // namespace extensions
