// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/supports_user_data.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

using content::BrowserThread;

namespace {

SpellcheckService::SpellCheckerBinder& GetSpellCheckerBinderOverride() {
  static base::NoDestructor<SpellcheckService::SpellCheckerBinder> binder;
  return *binder;
}

// Only record spelling-configuration metrics for profiles in which the user
// can configure spelling.
bool RecordSpellingConfigurationMetrics(content::BrowserContext* context) {
  return profiles::IsRegularUserProfile(Profile::FromBrowserContext(context));
}

}  // namespace

// TODO(rlp): I do not like globals, but keeping these for now during
// transition.
// An event used by browser tests to receive status events from this class and
// its derived classes.
base::WaitableEvent* g_status_event = nullptr;
SpellcheckService::EventType g_status_type =
    SpellcheckService::BDICT_NOTINITIALIZED;

SpellcheckService::SpellcheckService(content::BrowserContext* context)
    : context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  pref_change_registrar_.Init(prefs);
  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries, prefs);
  std::string first_of_dictionaries;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  // Ensure that the renderer always knows the platform spellchecking
  // language. This language is used for initialization of the text iterator.
  // If the iterator is not initialized, then the context menu does not show
  // spellcheck suggestions.
  // No migration is necessary, because the spellcheck language preference is
  // not user visible or modifiable in Chrome on Mac.
  dictionaries_pref.SetValue(std::vector<std::string>(
      1, spellcheck_platform::GetSpellCheckerLanguage()));
  first_of_dictionaries = dictionaries_pref.GetValue().front();
#else
  // Migrate preferences from single-language to multi-language schema.
  StringPrefMember single_dictionary_pref;
  single_dictionary_pref.Init(spellcheck::prefs::kSpellCheckDictionary, prefs);
  std::string single_dictionary = single_dictionary_pref.GetValue();

  if (!dictionaries_pref.GetValue().empty())
    first_of_dictionaries = dictionaries_pref.GetValue().front();

  if (first_of_dictionaries.empty() && !single_dictionary.empty()) {
    first_of_dictionaries = single_dictionary;
    dictionaries_pref.SetValue(
        std::vector<std::string>(1, first_of_dictionaries));
  }

  single_dictionary_pref.SetValue("");
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (!spellcheck::UseBrowserSpellChecker()) {
    // A user may have disabled the Windows spellcheck feature after adding
    // non-Hunspell supported languages on the language settings page. Remove
    // preferences for non-Hunspell languages so that there is no attempt to
    // load a non-existent Hunspell dictionary, and so that Hunspell
    // spellchecking isn't broken because of the failed load.
    ScopedListPrefUpdate update(prefs,
                                spellcheck::prefs::kSpellCheckDictionaries);
    update->EraseIf([](const base::Value& entry) {
      return spellcheck::GetCorrespondingSpellCheckLanguage(entry.GetString())
          .empty();
    });
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckDictionaries,
      base::BindRepeating(&SpellcheckService::OnSpellCheckDictionariesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckForcedDictionaries,
      base::BindRepeating(&SpellcheckService::OnSpellCheckDictionariesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckBlocklistedDictionaries,
      base::BindRepeating(&SpellcheckService::OnSpellCheckDictionariesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckUseSpellingService,
      base::BindRepeating(&SpellcheckService::OnUseSpellingServiceChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      language::prefs::kAcceptLanguages,
      base::BindRepeating(&SpellcheckService::OnAcceptLanguagesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckEnable,
      base::BindRepeating(&SpellcheckService::InitForAllRenderers,
                          base::Unretained(this)));

  custom_dictionary_ =
      std::make_unique<SpellcheckCustomDictionary>(context_->GetPath());
  custom_dictionary_->AddObserver(this);
  custom_dictionary_->Load();

#if BUILDFLAG(IS_WIN)
  if (spellcheck::UseBrowserSpellChecker() &&
      base::FeatureList::IsEnabled(
          spellcheck::kWinDelaySpellcheckServiceInit)) {
    // If initialization of the spellcheck service is on-demand, it is up to the
    // instantiator of the spellcheck service to call InitializeDictionaries
    // with a callback.
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  InitializeDictionaries(base::DoNothing());
}

SpellcheckService::~SpellcheckService() {
  // Remove pref observers
  pref_change_registrar_.RemoveAll();
}

base::WeakPtr<SpellcheckService> SpellcheckService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if !BUILDFLAG(IS_MAC)
// static
void SpellcheckService::GetDictionaries(
    content::BrowserContext* browser_context,
    std::vector<Dictionary>* dictionaries) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  std::set<std::string> spellcheck_dictionaries;
  for (const auto& value :
       prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries)) {
    const std::string* dictionary = value.GetIfString();
    if (dictionary)
      spellcheck_dictionaries.insert(*dictionary);
  }

  dictionaries->clear();
  std::vector<std::string> accept_languages =
      base::SplitString(prefs->GetString(language::prefs::kAcceptLanguages),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& accept_language : accept_languages) {
    Dictionary dictionary;
#if BUILDFLAG(IS_WIN)
    if (spellcheck::UseBrowserSpellChecker()) {
      SpellcheckService* spellcheck =
          SpellcheckServiceFactory::GetForContext(browser_context);
      if (spellcheck && spellcheck->UsesWindowsDictionary(accept_language))
        dictionary.language = accept_language;
    }

    if (dictionary.language.empty()) {
      dictionary.language =
          spellcheck::GetCorrespondingSpellCheckLanguage(accept_language);
    }
#else
    dictionary.language =
        spellcheck::GetCorrespondingSpellCheckLanguage(accept_language);
#endif  // BUILDFLAG(IS_WIN)

    if (dictionary.language.empty())
      continue;

    dictionary.used_for_spellcheck =
        spellcheck_dictionaries.count(dictionary.language) > 0;
    dictionaries->push_back(dictionary);
  }
}
#endif  // !BUILDFLAG(IS_MAC)

// static
bool SpellcheckService::SignalStatusEvent(
    SpellcheckService::EventType status_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_status_event)
    return false;
  g_status_type = status_type;
  g_status_event->Signal();
  return true;
}

// static
std::string SpellcheckService::GetSupportedAcceptLanguageCode(
    const std::string& supported_language_full_tag,
    bool generic_only) {
  // Default to accept language in hardcoded list of Hunspell dictionaries
  // (kSupportedSpellCheckerLanguages).
  std::string supported_accept_language =
      spellcheck::GetCorrespondingSpellCheckLanguage(
          supported_language_full_tag);
  if (generic_only) {
    supported_accept_language = SpellcheckService::GetLanguageAndScriptTag(
        supported_accept_language,
        /* include_script_tag= */ false);
  }

#if BUILDFLAG(IS_WIN)
  if (!spellcheck::UseBrowserSpellChecker())
    return supported_accept_language;

  // Exclude dictionaries that are for private use, such as "ja-Latn-JP-x-ext".
  if (SpellcheckService::HasPrivateUseSubTag(supported_language_full_tag))
    return "";

  // Collect the hardcoded list of accept-languages supported by the browser,
  // that is, languages that can be added as preferred languages in the
  // languages settings page.
  std::vector<std::string> accept_languages;
  l10n_util::GetAcceptLanguages(&accept_languages);

  if (generic_only) {
    return GetSupportedAcceptLanguageCodeGenericOnly(
        supported_language_full_tag, accept_languages);
  }

  // First try exact match. Per BCP47, tags are in ASCII and should be treated
  // as case-insensitive (although there are conventions for the capitalization
  // of subtags).
  auto iter = base::ranges::find_if(
      accept_languages,
      [supported_language_full_tag](const auto& accept_language) {
        return base::EqualsCaseInsensitiveASCII(supported_language_full_tag,
                                                accept_language);
      });
  if (iter != accept_languages.end())
    return *iter;

  // Then try matching just the language and (optional) script subtags, but
  // not the region subtag. For example, Edge supports sr-Cyrl-RS as an accept
  // language, but not sr-Cyrl-CS. Matching language + script subtags assures
  // we get the correct script for spellchecking, and not use sr-Latn-RS if
  // language packs for both scripts are installed on the system.
  if (!base::Contains(supported_language_full_tag, "-"))
    return "";

  iter = base::ranges::find_if(
      accept_languages,
      [supported_language_full_tag](const auto& accept_language) {
        return base::EqualsCaseInsensitiveASCII(
            SpellcheckService::GetLanguageAndScriptTag(
                supported_language_full_tag,
                /* include_script_tag= */ true),
            SpellcheckService::GetLanguageAndScriptTag(
                accept_language,
                /* include_script_tag= */ true));
      });

  if (iter != accept_languages.end())
    return *iter;

  // Then try just matching the leading language subtag. E.g. Edge supports
  // kok as an accept language, but if the Konkani language pack is
  // installed the Windows spellcheck API reports kok-Deva-IN for the
  // dictionary name.
  return GetSupportedAcceptLanguageCodeGenericOnly(supported_language_full_tag,
                                                   accept_languages);

#else
  return supported_accept_language;
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
// static
void SpellcheckService::EnableFirstUserLanguageForSpellcheck(
    PrefService* prefs) {
  // Ensure that spellcheck is enabled for the first language in the
  // accept languages list.
  base::Value::List user_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries).Clone();
  std::vector<std::string> user_languages =
      base::SplitString(prefs->GetString(language::prefs::kAcceptLanguages),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Some first run scenarios will add an accept language to preferences that
  // is not found in the hard-coded list in kAcceptLanguageList. Only
  // languages in kAcceptLanguageList can be spellchecked. An example is an
  // installation on a device where Finnish is the Windows display
  // language--the initial accept language preferences are observed to be
  // "fi-FI,fi,en-US,en". Only "fi" is contained in kAcceptLanguageList.
  std::string first_user_language;
  std::vector<std::string> accept_languages;
  l10n_util::GetAcceptLanguages(&accept_languages);
  for (const auto& user_language : user_languages) {
    if (base::Contains(accept_languages, user_language)) {
      first_user_language = user_language;
      break;
    }
  }

  bool first_user_language_spellchecked = false;
  for (const auto& dictionary_value : user_dictionaries) {
    first_user_language_spellchecked =
        base::Contains(dictionary_value.GetString(), first_user_language);
    if (first_user_language_spellchecked)
      break;
  }

  if (!first_user_language_spellchecked) {
    user_dictionaries.Insert(user_dictionaries.begin(),
                             base::Value(first_user_language));
    prefs->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                   std::move(user_dictionaries));
  }
}
#endif  // BUILDFLAG(IS_WIN)

void SpellcheckService::StartRecordingMetrics(bool spellcheck_enabled) {
  metrics_ = std::make_unique<SpellCheckHostMetrics>();
  auto record_configuration_metrics =
      RecordSpellingConfigurationMetrics(context_);
  if (record_configuration_metrics) {
    metrics_->RecordEnabledStats(spellcheck_enabled);
  }

  OnUseSpellingServiceChanged();

#if BUILDFLAG(IS_WIN)
  if (record_configuration_metrics) {
    RecordChromeLocalesStats();
    RecordSpellcheckLocalesStats();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void SpellcheckService::InitForRenderer(content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context = host->GetBrowserContext();
  if (SpellcheckServiceFactory::GetForContext(context) != this)
    return;

  const bool enable = IsSpellcheckEnabled();

  std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries;
  std::vector<std::string> custom_words;
  if (enable) {
    for (const auto& hunspell_dictionary : hunspell_dictionaries_) {
      dictionaries.push_back(spellcheck::mojom::SpellCheckBDictLanguage::New(
          hunspell_dictionary->GetDictionaryFile().Duplicate(),
          hunspell_dictionary->GetLanguage()));
    }

    custom_words.assign(custom_dictionary_->GetWords().begin(),
                        custom_dictionary_->GetWords().end());
  } else {
    // Disabling spell check should also disable spelling service.
    user_prefs::UserPrefs::Get(context)->SetBoolean(
        spellcheck::prefs::kSpellCheckUseSpellingService, false);
  }

  GetSpellCheckerForProcess(host)->Initialize(std::move(dictionaries),
                                              custom_words, enable);
}

SpellCheckHostMetrics* SpellcheckService::GetMetrics() const {
  return metrics_.get();
}

SpellcheckCustomDictionary* SpellcheckService::GetCustomDictionary() {
  return custom_dictionary_.get();
}

void SpellcheckService::LoadDictionaries() {
  hunspell_dictionaries_.clear();

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  const base::Value::List& user_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  const base::Value::List& forced_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckForcedDictionaries);

  // Build a lookup of blocked dictionaries to skip loading them.
  const base::Value::List& blocked_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckBlocklistedDictionaries);
  std::unordered_set<std::string> blocked_dictionaries_lookup;
  for (const auto& blocked_dict : blocked_dictionaries) {
    blocked_dictionaries_lookup.insert(blocked_dict.GetString());
  }

  // Merge both lists of dictionaries. Use a set to avoid duplicates.
  std::set<std::string> dictionaries;
  for (const auto& dictionary_value : user_dictionaries) {
    if (blocked_dictionaries_lookup.find(dictionary_value.GetString()) ==
        blocked_dictionaries_lookup.end())
      dictionaries.insert(dictionary_value.GetString());
  }
  for (const auto& dictionary_value : forced_dictionaries) {
    dictionaries.insert(dictionary_value.GetString());
  }

  for (const auto& dictionary : dictionaries) {
    // The spellcheck language passed to platform APIs may differ from the
    // accept language.
    std::string platform_spellcheck_language;
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    if (spellcheck::UseBrowserSpellChecker()) {
      std::string windows_dictionary_name =
          GetSupportedWindowsDictionaryLanguage(dictionary);
      if (!windows_dictionary_name.empty()) {
        platform_spellcheck_language =
            SpellcheckService::GetTagToPassToWindowsSpellchecker(
                dictionary, windows_dictionary_name);
      }
    }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    hunspell_dictionaries_.push_back(
        std::make_unique<SpellcheckHunspellDictionary>(
            dictionary, platform_spellcheck_language, context_, this));
    hunspell_dictionaries_.back()->AddObserver(this);
    hunspell_dictionaries_.back()->Load();
  }

#if BUILDFLAG(IS_WIN)
  if (RecordSpellingConfigurationMetrics(context_)) {
    RecordSpellcheckLocalesStats();
  }

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (base::FeatureList::IsEnabled(
          spellcheck::kWinDelaySpellcheckServiceInit) &&
      spellcheck::UseBrowserSpellChecker()) {
    // Only want to fire the callback on first call to LoadDictionaries
    // originating from InitializeDictionaries, since supported platform
    // dictionaries are cached throughout the browser session and not
    // dynamically updated. LoadDictionaries can be called multiple times in a
    // browser session, even before InitializeDictionaries is called, e.g. when
    // language settings are changed.
    if (!dictionaries_loaded() && dictionaries_loaded_callback_) {
      dictionaries_loaded_ = true;
      std::move(dictionaries_loaded_callback_).Run();
    }
    return;
  }
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#endif  // BUILDFLAG(IS_WIN)
  dictionaries_loaded_ = true;
}

const std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>&
SpellcheckService::GetHunspellDictionaries() {
  return hunspell_dictionaries_;
}

bool SpellcheckService::IsSpellcheckEnabled() const {
  const PrefService* prefs = user_prefs::UserPrefs::Get(context_);

  bool enable_if_uninitialized = false;
#if BUILDFLAG(IS_WIN)
  if (spellcheck::UseBrowserSpellChecker() &&
      base::FeatureList::IsEnabled(
          spellcheck::kWinDelaySpellcheckServiceInit)) {
    // If initialization of the spellcheck service is on-demand, the
    // renderer-side SpellCheck object needs to start out as enabled in order
    // for a click on editable content to initialize the spellcheck service.
    if (!dictionaries_loaded())
      enable_if_uninitialized = true;
  }
#endif  // BUILDFLAG(IS_WIN)

  return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable) &&
         (!hunspell_dictionaries_.empty() || enable_if_uninitialized);
}

void SpellcheckService::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  InitForRenderer(host);
}

void SpellcheckService::OnCustomDictionaryLoaded() {
  InitForAllRenderers();
}

void SpellcheckService::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::vector<std::string> additions(change.to_add().begin(),
                                           change.to_add().end());
  const std::vector<std::string> deletions(change.to_remove().begin(),
                                           change.to_remove().end());
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* process = it.GetCurrentValue();
    if (!process->IsInitializedAndNotDead())
      continue;
    GetSpellCheckerForProcess(process)->CustomDictionaryChanged(additions,
                                                                deletions);
  }
}

void SpellcheckService::OnHunspellDictionaryInitialized(
    const std::string& language) {
  InitForAllRenderers();
}

void SpellcheckService::OnHunspellDictionaryDownloadBegin(
    const std::string& language) {
}

void SpellcheckService::OnHunspellDictionaryDownloadSuccess(
    const std::string& language) {
}

void SpellcheckService::OnHunspellDictionaryDownloadFailure(
    const std::string& language) {
}

void SpellcheckService::InitializeDictionaries(base::OnceClosure done) {
  // The dictionaries only need to be initialized once.
  if (dictionaries_loaded()) {
    std::move(done).Run();
    return;
  }

#if BUILDFLAG(IS_WIN)
  dictionaries_loaded_callback_ = std::move(done);
  // Need to initialize the platform spellchecker in order to record platform
  // locale stats even if the platform spellcheck feature is disabled.
  InitializePlatformSpellchecker();
#endif  // BUILDFLAG(IS_WIN)

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  // Instantiates Metrics object for spellchecking to use.
  StartRecordingMetrics(
      prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable));

#if BUILDFLAG(IS_WIN)
  if (spellcheck::UseBrowserSpellChecker() && platform_spell_checker()) {
    spellcheck_platform::RetrieveSpellcheckLanguages(
        platform_spell_checker(),
        base::BindOnce(&SpellcheckService::InitWindowsDictionaryLanguages,
                       GetWeakPtr()));
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  // Using Hunspell.
  LoadDictionaries();
}

#if BUILDFLAG(IS_WIN)
void SpellcheckService::InitWindowsDictionaryLanguages(
    const std::vector<std::string>& windows_spellcheck_languages) {
  windows_spellcheck_dictionary_map_.clear();
  for (const auto& windows_spellcheck_language : windows_spellcheck_languages) {
    std::string accept_language =
        SpellcheckService::GetSupportedAcceptLanguageCode(
            windows_spellcheck_language, /* generic_only */ false);
    AddWindowsSpellcheckDictionary(accept_language,
                                   windows_spellcheck_language);

    // There is one unfortunate special case (so far the only one known). The
    // accept language "sr" is supported, and if you use it as a display
    // language you see Cyrillic script. If a Windows language pack is
    // installed that supports "sr-Cyrl-*", mark the "sr" accept language
    // as having Windows spellcheck support instead of using Hunspell.
    if (base::EqualsCaseInsensitiveASCII(
            "sr-Cyrl", SpellcheckService::GetLanguageAndScriptTag(
                           windows_spellcheck_language,
                           /* include_script_tag= */ true))) {
      AddWindowsSpellcheckDictionary("sr", windows_spellcheck_language);
    }

    // Add the generic language with the region subtag removed too if it exists
    // in the list of accept languages, and use it when calling the Windows
    // spellcheck APIs. For example, if the preferred language settings include
    // just generic Portuguese (pt), but the Portuguese (Brazil) platform
    // language pack (pt-BR) is installed, we want an entry for it so that the
    // generic Portuguese language can be enabled for spellchecking. The Windows
    // platform spellcheck API has logic to load the pt-BR dictionary if only pt
    // is specified as the BCP47 language tag. The use of a map in
    // AddWindowsSpellcheckDictionary ensures there won't be duplicate entries
    // if a generic language was already added above (ar-SA would already be
    // mapped to ar since the accept language ar-SA is not recognized by the
    // browser e.g.).
    accept_language = SpellcheckService::GetSupportedAcceptLanguageCode(
        windows_spellcheck_language, /* generic_only */ true);
    AddWindowsSpellcheckDictionary(accept_language, accept_language);
  }

  // A user may have removed a language pack for a non-Hunspell language after
  // enabling it for spellcheck on the language settings page. Remove
  // preferences for this language so that there is no attempt to load a
  // non-existent Hunspell dictionary, and so that Hunspell spellchecking isn't
  // broken because of the failed load. This also handles the case where the
  // primary preferred language is enabled for spellchecking during first run,
  // but it's now determined that there is neither Windows platform nor Hunspell
  // dictionary support for that language.
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);
  // When following object goes out of scope, preference change observers will
  // be notified (even if there is no preference change).
  ScopedListPrefUpdate update(prefs,
                              spellcheck::prefs::kSpellCheckDictionaries);
  update->EraseIf([this](const base::Value& entry) {
    const std::string dictionary_name = entry.GetString();
    return (!UsesWindowsDictionary(dictionary_name) &&
            spellcheck::GetCorrespondingSpellCheckLanguage(dictionary_name)
                .empty());
  });

  // No need to call LoadDictionaries() as when the ScopedListPrefUpdate object
  // goes out of scope, the preference change handler will do this.
}

bool SpellcheckService::UsesWindowsDictionary(
    std::string accept_language) const {
  return !GetSupportedWindowsDictionaryLanguage(accept_language).empty();
}
#endif  // BUILDFLAG(IS_WIN)

// static
void SpellcheckService::OverrideBinderForTesting(SpellCheckerBinder binder) {
  GetSpellCheckerBinderOverride() = std::move(binder);
}

// static
std::string SpellcheckService::GetLanguageAndScriptTag(
    const std::string& full_tag,
    bool include_script_tag) {
  if (full_tag.empty())
    return "";

  std::string language_and_script_tag;

  std::vector<std::string> subtags = base::SplitString(
      full_tag, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // Language subtag is required, all others optional.
  DCHECK_GE(subtags.size(), 1ULL);
  std::vector<std::string> subtag_tokens_to_pass;
  subtag_tokens_to_pass.push_back(subtags.front());
  subtags.erase(subtags.begin());

  // The optional script subtag always follows the language subtag, and is 4
  // characters in length.
  if (include_script_tag) {
    if (!subtags.empty() && subtags.front().length() == 4) {
      subtag_tokens_to_pass.push_back(subtags.front());
    }
  }

  language_and_script_tag = base::JoinString(subtag_tokens_to_pass, "-");

  return language_and_script_tag;
}

#if BUILDFLAG(IS_WIN)
// static
std::string SpellcheckService::GetSupportedAcceptLanguageCodeGenericOnly(
    const std::string& supported_language_full_tag,
    const std::vector<std::string>& accept_languages) {
  auto iter = base::ranges::find_if(
      accept_languages,
      [supported_language_full_tag](const auto& accept_language) {
        return base::EqualsCaseInsensitiveASCII(
            SpellcheckService::GetLanguageAndScriptTag(
                supported_language_full_tag,
                /* include_script_tag= */ false),
            SpellcheckService::GetLanguageAndScriptTag(
                accept_language,
                /* include_script_tag= */ false));
      });

  if (iter != accept_languages.end()) {
    // Special case for Serbian--"sr" implies Cyrillic script. Don't mark it as
    // supported for sr-Latn*.
    if (base::EqualsCaseInsensitiveASCII(
            SpellcheckService::GetLanguageAndScriptTag(
                supported_language_full_tag,
                /* include_script_tag= */ true),
            "sr-Latn")) {
      return "";
    }
    return *iter;
  }

  return "";
}

// static
bool SpellcheckService::HasPrivateUseSubTag(const std::string& full_tag) {
  std::vector<std::string> subtags = base::SplitString(
      full_tag, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Private use subtags are separated from the other subtags by the reserved
  // single-character subtag 'x'.
  return base::Contains(subtags, "x");
}

// static
std::string SpellcheckService::GetTagToPassToWindowsSpellchecker(
    const std::string& accept_language,
    const std::string& supported_language_full_tag) {
  // First try exact match. Per BCP47, tags are in ASCII and should be treated
  // as case-insensitive (although there are conventions for the capitalization
  // of subtags, they are sometimes broken).
  if (base::EqualsCaseInsensitiveASCII(supported_language_full_tag,
                                       accept_language)) {
    // Unambiguous spellcheck dictionary to be used.
    return supported_language_full_tag;
  }

  // Accept language does not match script or region subtags.
  // If there is a script subtag, include it, to avoid for example passing
  // "sr" which is ambiguous as Serbian can use Cyrillic or Latin script.
  // There is one unfortunate special case (so far the only one known). The
  // accept language "sr" is supported, and if you use it as a display
  // language you see Cyrillic script. However, the Windows spellcheck API
  // returns "sr-Latn-*" dictionaries if the unqualified language tag is
  // passed. The following forces Windows spellchecking to use Cyrillic script
  // in this case, and if the language pack is not installed there will be a
  // fallback to Hunspell support when spellchecking is performed.
  if (base::EqualsCaseInsensitiveASCII("sr", accept_language))
    return "sr-Cyrl";

  return SpellcheckService::GetLanguageAndScriptTag(
      supported_language_full_tag,
      /* include_script_tag= */ true);
}

#endif  // BUILDFLAG(IS_WIN)

// static
void SpellcheckService::AttachStatusEvent(base::WaitableEvent* status_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  g_status_event = status_event;
}

// static
SpellcheckService::EventType SpellcheckService::GetStatusEvent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_status_type;
}

mojo::Remote<spellcheck::mojom::SpellChecker>
SpellcheckService::GetSpellCheckerForProcess(content::RenderProcessHost* host) {
  mojo::Remote<spellcheck::mojom::SpellChecker> spellchecker;
  auto receiver = spellchecker.BindNewPipeAndPassReceiver();
  auto binder = GetSpellCheckerBinderOverride();
  if (binder)
    binder.Run(std::move(receiver));
  else
    host->BindReceiver(std::move(receiver));
  return spellchecker;
}

void SpellcheckService::InitForAllRenderers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    if (process && process->GetProcess().Handle())
      InitForRenderer(process);
  }
}

void SpellcheckService::OnSpellCheckDictionariesChanged() {
  // If there are hunspell dictionaries, then fire off notifications to the
  // renderers after the dictionaries are finished loading.
  LoadDictionaries();

  // If there are no hunspell dictionaries to load, then immediately let the
  // renderers know the new state.
  if (hunspell_dictionaries_.empty()) {
#if !BUILDFLAG(IS_MAC)
    // Only update non-MacOS platform because basic spell check on Mac OS
    // is controlled by OS and doesn't depend on users' dictionaries pref
    user_prefs::UserPrefs::Get(context_)->SetBoolean(
        spellcheck::prefs::kSpellCheckEnable, false);
#endif  // !BUILDFLAG(IS_MAC)
    InitForAllRenderers();
  }
}

void SpellcheckService::OnUseSpellingServiceChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService);
  if (metrics_ && RecordSpellingConfigurationMetrics(context_)) {
    metrics_->RecordSpellingServiceStats(enabled);
  }
}

void SpellcheckService::OnAcceptLanguagesChanged() {
  // Accept-Languages and spell check are decoupled on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::vector<std::string> accept_languages = GetNormalizedAcceptLanguages();

  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries,
                         user_prefs::UserPrefs::Get(context_));
  std::vector<std::string> dictionaries = dictionaries_pref.GetValue();
  std::vector<std::string> filtered_dictionaries;

  for (const auto& dictionary : dictionaries) {
    if (base::Contains(accept_languages, dictionary)) {
      filtered_dictionaries.push_back(dictionary);
    }
  }

  dictionaries_pref.SetValue(filtered_dictionaries);

#if BUILDFLAG(IS_WIN)
  if (RecordSpellingConfigurationMetrics(context_)) {
    RecordChromeLocalesStats();
  }
#endif  // BUILDFLAG(IS_WIN)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

std::vector<std::string> SpellcheckService::GetNormalizedAcceptLanguages(
    bool normalize_for_spellcheck) const {
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  std::vector<std::string> accept_languages =
      base::SplitString(prefs->GetString(language::prefs::kAcceptLanguages),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (normalize_for_spellcheck) {
    base::ranges::transform(
        accept_languages, accept_languages.begin(),
        [&](const std::string& language) {
#if BUILDFLAG(IS_WIN)
          if (spellcheck::UseBrowserSpellChecker() &&
              UsesWindowsDictionary(language))
            return language;
#endif  // BUILDFLAG(IS_WIN)
          return spellcheck::GetCorrespondingSpellCheckLanguage(language);
        });
  }

  return accept_languages;
}

#if BUILDFLAG(IS_WIN)
void SpellcheckService::InitializePlatformSpellchecker() {
  // The Windows spell checker must be created before the dictionaries are
  // initialized. Note it is instantiated even if only Hunspell is being used
  // since metrics on the availability of Windows platform language packs are
  // being recorded. Thus method should only be called once, except in test
  // code.
  if (!platform_spell_checker()) {
    platform_spell_checker_ = std::make_unique<WindowsSpellChecker>(
        base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()}));
  }
}

void SpellcheckService::RecordSpellcheckLocalesStats() {
  if (metrics_ && platform_spell_checker() && !hunspell_dictionaries_.empty()) {
    std::vector<std::string> hunspell_locales;
    for (auto& dict : hunspell_dictionaries_) {
      hunspell_locales.push_back(dict->GetLanguage());
    }
    spellcheck_platform::RecordSpellcheckLocalesStats(
        platform_spell_checker(), std::move(hunspell_locales));
  }
}

void SpellcheckService::RecordChromeLocalesStats() {
  if (metrics_ && platform_spell_checker()) {
    std::vector<std::string> accept_languages =
        GetNormalizedAcceptLanguages(/* normalize_for_spellcheck */ false);
    if (!accept_languages.empty()) {
      spellcheck_platform::RecordChromeLocalesStats(
          platform_spell_checker(), std::move(accept_languages));
    }
  }
}

void SpellcheckService::AddWindowsSpellcheckDictionary(
    const std::string& accept_language,
    const std::string& supported_language_full_tag) {
  if (!accept_language.empty()) {
    windows_spellcheck_dictionary_map_.insert(
        {accept_language, supported_language_full_tag});
  }
}

std::string SpellcheckService::GetSupportedWindowsDictionaryLanguage(
    const std::string& accept_language) const {
  // BCP47 language tag used by the Windows spellchecker API.
  std::string spellcheck_language;

  auto it = windows_spellcheck_dictionary_map_.find(accept_language);
  if (it != windows_spellcheck_dictionary_map_.end())
    spellcheck_language = it->second;

  return spellcheck_language;
}

void SpellcheckService::AddSpellcheckLanguagesForTesting(
    const std::vector<std::string>& languages) {
  InitializePlatformSpellchecker();
  if (platform_spell_checker()) {
    spellcheck_platform::AddSpellcheckLanguagesForTesting(
        platform_spell_checker(), languages);
  }
}
#endif  // BUILDFLAG(IS_WIN)
