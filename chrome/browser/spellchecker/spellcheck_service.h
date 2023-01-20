// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/spellcheck/browser/platform_spell_checker.h"
#include "components/spellcheck/common/spellcheck.mojom-forward.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

class SpellCheckHostMetrics;

namespace base {
class WaitableEvent;
}

namespace content {
class BrowserContext;
class RenderProcessHost;
}

#if BUILDFLAG(IS_WIN)
namespace extensions {
class LanguageSettingsPrivateApiTestDelayInit;
}
#endif  // BUILDFLAG(IS_WIN)

// Encapsulates the browser side spellcheck service. There is one of these per
// profile and each is created by the SpellCheckServiceFactory.  The
// SpellcheckService maintains any per-profile information about spellcheck.
class SpellcheckService : public KeyedService,
                          public content::RenderProcessHostCreationObserver,
                          public SpellcheckCustomDictionary::Observer,
                          public SpellcheckHunspellDictionary::Observer {
 public:
  // Event types used for reporting the status of this class and its derived
  // classes to browser tests.
  enum EventType {
    BDICT_NOTINITIALIZED,
    BDICT_CORRUPTED,
  };

  // A dictionary that can be used for spellcheck.
  struct Dictionary {
    // The shortest unambiguous identifier for a language from
    // |g_supported_spellchecker_languages|. For example, "bg" for Bulgarian,
    // because Chrome has only one Bulgarian language. However, "en-US" for
    // English (United States), because Chrome has several versions of English.
    std::string language;

    // Whether |language| is currently used for spellcheck.
    bool used_for_spellcheck;
  };

  explicit SpellcheckService(content::BrowserContext* context);

  SpellcheckService(const SpellcheckService&) = delete;
  SpellcheckService& operator=(const SpellcheckService&) = delete;

  ~SpellcheckService() override;

  base::WeakPtr<SpellcheckService> GetWeakPtr();

#if !BUILDFLAG(IS_MAC)
  // Returns all currently configured |dictionaries| to display in the context
  // menu over a text area. The context menu is used for selecting the
  // dictionaries used for spellcheck.
  static void GetDictionaries(content::BrowserContext* browser_context,
                              std::vector<Dictionary>* dictionaries);
#endif  // !BUILDFLAG(IS_MAC)

  // Signals the event attached by AttachTestEvent() to report the specified
  // event to browser tests. This function is called by this class and its
  // derived classes to report their status. This function does not do anything
  // when we do not set an event to |status_event_|.
  static bool SignalStatusEvent(EventType type);

  // Get the best match of a supported accept language code for the provided
  // language tag. Returns an empty string if no match is found. Method cannot
  // be defined in spellcheck_common.h as it depends on l10n_util, and code
  // under components cannot depend on ui/base. If |generic_only| is true,
  // then only return the language subtag (first part of the full BCP47 tag)
  // if the generic accept language is supported by the browser.
  static std::string GetSupportedAcceptLanguageCode(
      const std::string& supported_language_full_tag,
      bool generic_only = false);

#if BUILDFLAG(IS_WIN)
  // Since Windows platform dictionary support is determined asynchronously,
  // this method is used to assure that the first preferred language initially
  // has spellchecking enabled after first run. Spellchecking for the primary
  // language will be disabled later if there is no dictionary support.
  static void EnableFirstUserLanguageForSpellcheck(PrefService* prefs);
#endif  // BUILDFLAG(IS_WIN)

  // Instantiates SpellCheckHostMetrics object and makes it ready for recording
  // metrics. This should be called only if the metrics recording is active.
  void StartRecordingMetrics(bool spellcheck_enabled);

  // Pass the renderer some basic initialization information. Note that the
  // renderer will not load Hunspell until it needs to.
  void InitForRenderer(content::RenderProcessHost* host);

  // Returns a metrics counter associated with this object,
  // or null when metrics recording is disabled.
  SpellCheckHostMetrics* GetMetrics() const;

  // Returns the instance of the custom dictionary.
  SpellcheckCustomDictionary* GetCustomDictionary();

  // Returns the instance of the vector of Hunspell dictionaries.
  const std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>&
  GetHunspellDictionaries();

  // Returns whether spellchecking is enabled in preferences and if there are
  // dictionaries available.
  bool IsSpellcheckEnabled() const;

  // content::RenderProcessHostCreationObserver implementation.
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& change) override;

  // SpellcheckHunspellDictionary::Observer implementation.
  void OnHunspellDictionaryInitialized(const std::string& language) override;
  void OnHunspellDictionaryDownloadBegin(const std::string& language) override;
  void OnHunspellDictionaryDownloadSuccess(
      const std::string& language) override;
  void OnHunspellDictionaryDownloadFailure(
      const std::string& language) override;

  // One-time initialization of dictionaries if needed.
  void InitializeDictionaries(base::OnceClosure done);

#if BUILDFLAG(IS_WIN)
  // Callback for spellcheck_platform::RetrieveSpellcheckLanguages. Populates
  // map of preferred languages to available platform dictionaries then
  // loads the dictionaries.
  void InitWindowsDictionaryLanguages(
      const std::vector<std::string>& windows_spellcheck_languages);

  // Indicates whether given accept language has Windows spellcheck platform
  // support.
  bool UsesWindowsDictionary(std::string accept_language) const;
#endif  // BUILDFLAG(IS_WIN)

  // The returned pointer can be null if the current platform doesn't need a
  // per-profile, platform-specific spell check object. Currently, only Windows
  // requires one.
  PlatformSpellChecker* platform_spell_checker() {
    return platform_spell_checker_.get();
  }

  // Indicates whether dictionaries have been loaded initially.
  bool dictionaries_loaded() const { return dictionaries_loaded_; }

  // Allows tests to override how SpellcheckService binds its interface
  // receiver, instead of going through a RenderProcessHost by default.
  using SpellCheckerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<spellcheck::mojom::SpellChecker>)>;
  static void OverrideBinderForTesting(SpellCheckerBinder binder);

 private:
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT);
#if BUILDFLAG(IS_WIN)
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceWindowsHybridBrowserTest,
                           WindowsHybridSpellcheck);
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceWindowsHybridBrowserTestDelayInit,
                           WindowsHybridSpellcheckDelayInit);
  friend class SpellcheckServiceHybridUnitTestBase;
  friend class SpellcheckServiceHybridUnitTestDelayInitBase;
  friend class extensions::LanguageSettingsPrivateApiTestDelayInit;
#endif  // BUILDFLAG(IS_WIN)

  // Starts the process of loading the dictionaries (Hunspell and platform). Can
  // be called multiple times in a browser session if spellcheck settings
  // change.
  void LoadDictionaries();

  // Parses a full BCP47 language tag to return just the language subtag,
  // optionally with a hyphen and script subtag appended.
  static std::string GetLanguageAndScriptTag(const std::string& full_tag,
                                             bool include_script_tag);

#if BUILDFLAG(IS_WIN)
  // Returns the language subtag (first part of the full BCP47 tag)
  // if the generic accept language is supported by the browser.
  static std::string GetSupportedAcceptLanguageCodeGenericOnly(
      const std::string& supported_language_full_tag,
      const std::vector<std::string>& accept_languages);

  // Returns true if full BCP47 language tag contains private use subtag (e.g in
  // the tag "ja-Latn-JP-x-ext"), indicating the tag is only for use by private
  // agreement.
  static bool HasPrivateUseSubTag(const std::string& full_tag);

  // Returns the BCP47 language tag to pass to the Windows spellcheck API, based
  // on the accept language and full tag, with special logic for languages that
  // can be written in different scripts.
  static std::string GetTagToPassToWindowsSpellchecker(
      const std::string& accept_language,
      const std::string& supported_language_full_tag);
#endif  // BUILDFLAG(IS_WIN)

  // Attaches an event so browser tests can listen the status events.
  static void AttachStatusEvent(base::WaitableEvent* status_event);

  // Returns the status event type.
  static EventType GetStatusEvent();

  mojo::Remote<spellcheck::mojom::SpellChecker> GetSpellCheckerForProcess(
      content::RenderProcessHost* host);

  // Pass all renderers some basic initialization information.
  void InitForAllRenderers();

  // Reacts to a change in user preference on which languages should be used for
  // spellchecking.
  void OnSpellCheckDictionariesChanged();

  // Notification handler for changes to prefs::kSpellCheckUseSpellingService.
  void OnUseSpellingServiceChanged();

  // Notification handler for changes to prefs::kAcceptLanguages. Removes from
  // prefs::kSpellCheckDictionaries any languages that are not in
  // prefs::kAcceptLanguages.
  void OnAcceptLanguagesChanged();

  // Gets the user languages from the accept_languages pref and trims them of
  // leading and trailing whitespaces. If |normalize_for_spellcheck| is |true|,
  // also normalizes the format to xx or xx-YY based on the list of spell check
  // languages supported by Hunspell. Note that if |normalize_for_spellcheck| is
  // |true|, languages not supported by Hunspell will be returned as empty
  // strings.
  std::vector<std::string> GetNormalizedAcceptLanguages(
      bool normalize_for_spellcheck = true) const;

#if BUILDFLAG(IS_WIN)
  // Initializes the platform spell checker.
  void InitializePlatformSpellchecker();

  // Records statistics about spell check support for the user's Chrome locales.
  void RecordChromeLocalesStats();

  // Records statistics about which spell checker supports which of the user's
  // enabled spell check locales.
  void RecordSpellcheckLocalesStats();

  // Adds an item to the cached collection mapping an accept language from
  // language settings to a BCP47 language tag to be passed to the Windows
  // spellchecker API, guarding against duplicate entries for the same accept
  // language.
  void AddWindowsSpellcheckDictionary(
      const std::string& accept_language,
      const std::string& supported_language_full_tag);

  // Gets the BCP47 language tag to pass to Windows spellcheck API, by
  // searching through the collection of languages already known to have
  // Windows spellchecker support on the system. Can return an empty string
  // if there is no Windows spellchecker support for this language on the
  // system.
  std::string GetSupportedWindowsDictionaryLanguage(
      const std::string& accept_language) const;

  // Test-only method for adding fake list of platform spellcheck languages
  // before calling InitializeDictionaries().
  void AddSpellcheckLanguagesForTesting(
      const std::vector<std::string>& languages);
#endif  // BUILDFLAG(IS_WIN)

  // WindowsSpellChecker must be created before the dictionary instantiation and
  // destroyed after dictionary destruction.
  std::unique_ptr<PlatformSpellChecker> platform_spell_checker_;

  PrefChangeRegistrar pref_change_registrar_;

  // A pointer to the BrowserContext which this service refers to.
  raw_ptr<content::BrowserContext> context_;

  std::unique_ptr<SpellCheckHostMetrics> metrics_;

  std::unique_ptr<SpellcheckCustomDictionary> custom_dictionary_;

  std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>
      hunspell_dictionaries_;

#if BUILDFLAG(IS_WIN)
  // Maps accept language tags to Windows spellcheck BCP47 tags, an analog
  // of the hardcoded kSupportedSpellCheckerLanguages used for Hunspell,
  // with the difference that only language packs installed on the system
  // with spellchecker support are included.
  std::map<std::string, std::string> windows_spellcheck_dictionary_map_;

  // Callback passed as argument to InitializeDictionaries, and invoked when
  // the dictionaries are loaded for the first time.
  base::OnceClosure dictionaries_loaded_callback_;
#endif  // BUILDFLAG(IS_WIN)

  // Flag indicating dictionaries have been loaded initially.
  bool dictionaries_loaded_ = false;

  base::WeakPtrFactory<SpellcheckService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_
