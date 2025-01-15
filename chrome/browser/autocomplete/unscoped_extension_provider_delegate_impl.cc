// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/unscoped_extension_provider_delegate_impl.h"

#include <cstddef>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/omnibox/omnibox_input_watcher_factory.h"
#include "chrome/browser/omnibox/omnibox_suggestions_watcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/unscoped_extension_provider.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck

UnscopedExtensionProviderDelegateImpl::UnscopedExtensionProviderDelegateImpl(
    Profile* profile,
    UnscopedExtensionProvider* provider)
    : profile_(profile), provider_(provider) {
  CHECK(provider_);
  omnibox_input_observation_.Observe(
      OmniboxInputWatcherFactory::GetForBrowserContext(profile_));

  // TODO(crbug.com/40810217): figure out how this would work for incognito.
  omnibox_suggestions_observation_.Observe(
      OmniboxSuggestionsWatcherFactory::GetForBrowserContext(
          profile_->GetOriginalProfile()));
}

UnscopedExtensionProviderDelegateImpl::
    ~UnscopedExtensionProviderDelegateImpl() = default;

void UnscopedExtensionProviderDelegateImpl::Start(
    const AutocompleteInput& input,
    bool minimal_changes,
    std::set<std::string> unscoped_mode_extension_ids) {
  // Reset last suggest matches.
  extension_suggest_matches_.clear();
  provider_->set_done(false);

  for (const std::string& extension_id : unscoped_mode_extension_ids) {
    extensions::ExtensionOmniboxEventRouter::OnInputChanged(
        profile_, extension_id, base::UTF16ToUTF8(input.text()),
        current_request_id_);
  }
}

void UnscopedExtensionProviderDelegateImpl::IncrementRequestId() {
  current_request_id_++;
}

void UnscopedExtensionProviderDelegateImpl::OnOmniboxSuggestionsReady(
    extensions::api::omnibox::SendSuggestions::Params* suggestions,
    const std::string& extension_id) {
  CHECK(suggestions);

  if (suggestions->request_id != current_request_id_) {
    // This is an old result, so just ignore.
    return;
  }

  int first_relevance = 10000000;
  int relevance_increment = 1;

  for (const auto& suggestion : suggestions->suggest_results) {
    // TODO(379141010): calculate relevance.
    extension_suggest_matches_.push_back(CreateAutocompleteMatch(
        suggestion, first_relevance - relevance_increment, extension_id));
    relevance_increment++;
  }

  ACMatches* matches = provider_->matches();
  matches->insert(matches->end(), extension_suggest_matches_.begin(),
                  extension_suggest_matches_.end());
  provider_->set_done(true);
  provider_->NotifyListeners(!extension_suggest_matches_.empty());
}

// Input has been accepted, so we're done with this input session. Ensure
// we don't send the OnInputCancelled event, or handle any more stray
// suggestions_ready events.
void UnscopedExtensionProviderDelegateImpl::OnOmniboxInputEntered() {
  // TODO(378538411): make sure this called when a match created by this class
  // is selected.
  IncrementRequestId();
}

AutocompleteMatch
UnscopedExtensionProviderDelegateImpl::CreateAutocompleteMatch(
    const omnibox_api::SuggestResult& suggestion,
    int relevance,
    const std::string& extension_id) {
  AutocompleteMatch match(provider_.get(), relevance, false,
                          AutocompleteMatchType::SEARCH_OTHER_ENGINE);
  match.fill_into_edit = base::UTF8ToUTF16(suggestion.content);
  match.contents = base::UTF8ToUTF16(suggestion.description);
  match.contents_class.emplace_back(0, ACMatchClassification::DIM);
  match.transition = ui::PAGE_TRANSITION_GENERATED;

  TemplateURLService* model = provider_->GetTemplateURLService();
  const TemplateURL* template_url = model->FindTemplateURLForExtension(
      extension_id, TemplateURL::OMNIBOX_API_EXTENSION);

  match.keyword = template_url->keyword();

  // The destination_url will not be used for navigation, but it needs to be set
  // for de-duplication, shortcuts provider, and other logic in
  // `OmniboxEditModel::OpenMatch()`.
  const TemplateURLRef& element_ref = template_url->url_ref();
  CHECK(element_ref.SupportsReplacement(
      provider_->GetTemplateURLService()->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_terms_args(
      base::UTF8ToUTF16(suggestion.content));
  match.destination_url = GURL(element_ref.ReplaceSearchTerms(
      search_terms_args,
      provider_->GetTemplateURLService()->search_terms_data()));

  match.contents_class =
      extensions::StyleTypesToACMatchClassifications(suggestion);
  return match;
}
