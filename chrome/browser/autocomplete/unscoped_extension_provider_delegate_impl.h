// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_

#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"
#include "components/omnibox/browser/unscoped_extension_provider.h"
#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Should not be included when extensions are disabled"
#endif

namespace omnibox_api = extensions::api::omnibox;

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
  void IncrementRequestId() override;

  // OmniboxInputWatcher::Observer:
  void OnOmniboxInputEntered() override;
  // OmniboxSuggestionsWatcher::Observer:
  void OnOmniboxSuggestionsReady(
      omnibox_api::SendSuggestions::Params* suggestions,
      const std::string& extension_id) override;

 private:
  // Creates an `AutocompleteMatch` for the suggestion.
  AutocompleteMatch CreateAutocompleteMatch(
      const omnibox_api::SuggestResult& suggestion,
      int relevance,
      const std::string& extension_id);

  // Resets all state related to suggestion group mapping.
  void ResetSuggestionGroupsMap();

  // Identifies the current input state. This is incremented each time the
  // autocomplete edit's input changes in any way. It is used to tell
  // whether suggest results from the extension are current.
  int current_request_id_ = 0;

  // TODO(378538411): populate this once the suggestions logic is implemented.
  //  Saved suggestions that were received from the extension used
  //  for resetting matches without asking the extension again.
  std::vector<AutocompleteMatch> extension_suggest_matches_;

  // Next group available to be given to a set of extension suggestions.
  // Possible groups are defined in `kReservedGroupIdMap`.
  int next_available_group_index_ = 0;

  // Maps extension id to a group. Allows extensions to have distinct headers.
  std::unordered_map<extensions::ExtensionId, omnibox::GroupId>
      extension_id_group_map_;

  raw_ptr<Profile> profile_;

  // The owner of this class.
  raw_ptr<UnscopedExtensionProvider> provider_;

  base::ScopedObservation<OmniboxInputWatcher, OmniboxInputWatcher::Observer>
      omnibox_input_observation_{this};
  base::ScopedObservation<OmniboxSuggestionsWatcher,
                          OmniboxSuggestionsWatcher::Observer>
      omnibox_suggestions_observation_{this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_
