// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// KeywordExtensionsDelegateImpl contains the extensions-only logic used by
// KeywordProvider. Overrides KeywordExtensionsDelegate which does nothing.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"
#include "components/omnibox/browser/omnibox_suggestions_watcher.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Should not be included when extensions are disabled"
#endif

class Profile;

class KeywordExtensionsDelegateImpl
    : public KeywordExtensionsDelegate,
      public OmniboxInputWatcher::Observer,
      public OmniboxSuggestionsWatcher::Observer {
 public:
  KeywordExtensionsDelegateImpl(Profile* profile, KeywordProvider* provider);

  KeywordExtensionsDelegateImpl(const KeywordExtensionsDelegateImpl&) = delete;
  KeywordExtensionsDelegateImpl& operator=(
      const KeywordExtensionsDelegateImpl&) = delete;

  ~KeywordExtensionsDelegateImpl() override;

  // KeywordExtensionsDelegate:
  void DeleteSuggestion(const TemplateURL* template_url,
                        const std::u16string& suggestion_text) override;

 private:
  // KeywordExtensionsDelegate:
  void IncrementInputId() override;
  bool IsEnabledExtension(const std::string& extension_id) override;
  bool Start(const AutocompleteInput& input,
             bool minimal_changes,
             const TemplateURL* template_url,
             const std::u16string& remaining_input) override;
  void EnterExtensionKeywordMode(const std::string& extension_id) override;
  void MaybeEndExtensionKeywordMode() override;

  // OmniboxInputWatcher::Observer:
  void OnOmniboxInputEntered() override;
  // OmniboxSuggestionsWatcher::Observer:
  void OnOmniboxSuggestionsReady(
      extensions::api::omnibox::SendSuggestions::Params* suggestions) override;
  void OnOmniboxDefaultSuggestionChanged() override;

  ACMatches* matches() { return &provider_->matches_; }
  void set_done(bool done) {
    provider_->done_ = done;
  }

  // Notifies the KeywordProvider about asynchronous updates from the extension.
  void OnProviderUpdate(bool updated_matches);

  // Identifies the current input state. This is incremented each time the
  // autocomplete edit's input changes in any way. It is used to tell whether
  // suggest results from the extension are current.
  int current_input_id_;

  // The input state at the time we last asked the extension for suggest
  // results.
  AutocompleteInput extension_suggest_last_input_;

  // We remember the last suggestions we've received from the extension in case
  // we need to reset our matches without asking the extension again.
  std::vector<AutocompleteMatch> extension_suggest_matches_;

  // If non-empty, holds the ID of the extension whose keyword is currently in
  // the URL bar while the autocomplete popup is open.
  std::string current_keyword_extension_id_;

  raw_ptr<Profile> profile_;

  // The owner of this class.
  raw_ptr<KeywordProvider> provider_;

  // We need our input IDs to be unique across all profiles, so we keep a global
  // UID that each provider uses.
  static int global_input_uid_;

  base::ScopedObservation<OmniboxInputWatcher, OmniboxInputWatcher::Observer>
      omnibox_input_observation_{this};
  base::ScopedObservation<OmniboxSuggestionsWatcher,
                          OmniboxSuggestionsWatcher::Observer>
      omnibox_suggestions_observation_{this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_
