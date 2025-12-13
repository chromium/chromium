// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_

#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"
#include "components/omnibox/browser/unscoped_extension_provider.h"
#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#error "Should not be included when extensions are disabled"
#endif

class UnscopedExtensionProvider;
class Profile;

// Delegate observes extension events as well as input changes in the omnibox.
// This class is considered the intermediary between the provider (in
// components/) and the Omnibox API (in chrome/browser).
class UnscopedExtensionProviderDelegateImpl
    : public UnscopedExtensionProviderDelegate,
      public OmniboxInputWatcher::Observer,
      public OmniboxSuggestionsWatcher::Observer {
 public:
  UnscopedExtensionProviderDelegateImpl(Profile* profile,
                                        UnscopedExtensionProvider* provider);
  UnscopedExtensionProviderDelegateImpl(
      const UnscopedExtensionProviderDelegateImpl&) = delete;
  UnscopedExtensionProviderDelegateImpl& operator=(
      const UnscopedExtensionProviderDelegateImpl&) = delete;
  ~UnscopedExtensionProviderDelegateImpl() override;

  // UnscopedExtensionProviderDelegate:
  void Start(const AutocompleteInput& input,
             bool minimal_changes,
             std::set<std::string> unscoped_mode_extension_ids) override;
  void Stop(bool clear_cached_results) override;
  void DeleteSuggestion(const TemplateURL* template_url,
                        const std::u16string& suggestion_text) override;

  // OmniboxInputWatcher::Observer:
  void OnOmniboxInputEntered() override;
  // OmniboxSuggestionsWatcher::Observer:
  void OnOmniboxSuggestionsReady(
      const std::vector<ExtensionSuggestion>& suggestions,
      const int request_id,
      const std::string& extension_id) override;

 private:
  // Creates an `AutocompleteMatch` for the suggestion.
  AutocompleteMatch CreateAutocompleteMatch(
      const ExtensionSuggestion& suggestion,
      int relevance,
      const std::string& extension_id);

  // Returns true if an extension is enabled.
  bool IsEnabledExtension(const std::string& extension_id);

  // Clears the current list of cached matches and suggestion group information.
  void ClearSuggestions();

  void OnActionExecuted(const std::string& extension_id,
                        const std::string& action_name,
                        const std::string& contents);

  // Incremented each time a new request for suggestions is sent to extensions
  // or when the input is accepted. Used to discard any suggestions that may be
  // incoming later with a stale request ID.
  int current_request_id_ = 0;

  // The first relevance score to assign to the suggestions for the current
  // request for suggestions.
  int first_suggestion_relevance_ = 0;

  // Current list of matches received from the extensions. Used to update the
  // list of matches in the provider.
  std::vector<AutocompleteMatch> extension_suggest_matches_;

  // Next group available to be given to a set of extension suggestions.
  // Possible groups are defined in `kReservedGroupIdMap`.
  size_t next_available_group_index_ = 0;
  // Next section available to be given to a set of extension suggestions.
  // Possible sections are defined in `kReservedSectionMap`.
  size_t next_available_section_index_ = 0;

  // Maps extension IDs to group IDs. Allows suggestions from different
  // extensions to have distinct headers.
  std::unordered_map<extensions::ExtensionId, omnibox::GroupId>
      extension_id_to_group_id_map_;

  raw_ptr<Profile> profile_;

  // The owner of this class.
  raw_ptr<UnscopedExtensionProvider> provider_;

  base::ScopedObservation<OmniboxInputWatcher, OmniboxInputWatcher::Observer>
      omnibox_input_observation_{this};
  base::ScopedObservation<OmniboxSuggestionsWatcher,
                          OmniboxSuggestionsWatcher::Observer>
      omnibox_suggestions_observation_{this};

  base::WeakPtrFactory<UnscopedExtensionProviderDelegateImpl> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_
