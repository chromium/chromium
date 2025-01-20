// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/unscoped_extension_provider_delegate_impl.h"

#include <cstddef>
#include <string>

#include "base/containers/fixed_flat_map.h"
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

namespace {
constexpr auto kReservedGroupIdMap =
    base::MakeFixedFlatMap<size_t, omnibox::GroupId>(
        {{0, omnibox::GROUP_UNSCOPED_EXTENSION_1},
         {1, omnibox::GROUP_UNSCOPED_EXTENSION_2}});
constexpr auto kReservedSectionMap =
    base::MakeFixedFlatMap<size_t, omnibox::GroupSection>(
        {{0, omnibox::SECTION_UNSCOPED_EXTENSION_1},
         {1, omnibox::SECTION_UNSCOPED_EXTENSION_2}});
}  // namespace

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
  CHECK(extension_suggest_matches_.empty());
  CHECK(extension_id_to_group_id_map_.empty());

  provider_->set_done(false);

  for (const std::string& extension_id : unscoped_mode_extension_ids) {
    extensions::ExtensionOmniboxEventRouter::OnInputChanged(
        profile_, extension_id, base::UTF16ToUTF8(input.text()),
        current_request_id_);
  }
}

void UnscopedExtensionProviderDelegateImpl::Stop(bool clear_cached_results) {
  current_request_id_++;
  if (clear_cached_results) {
    ClearSuggestions();
  }
}

void UnscopedExtensionProviderDelegateImpl::OnOmniboxSuggestionsReady(
    extensions::api::omnibox::SendSuggestions::Params* suggestions,
    const std::string& extension_id) {
  CHECK(suggestions);

  // Discard suggestions with a stale request ID.
  if (suggestions->request_id != current_request_id_) {
    return;
  }

  TemplateURLService* turl_service = provider_->GetTemplateURLService();
  const TemplateURL* template_url = turl_service->FindTemplateURLForExtension(
      extension_id, TemplateURL::OMNIBOX_API_EXTENSION);

  if (!base::Contains(extension_id_to_group_id_map_, extension_id)) {
    if (next_available_group_index_ == kReservedGroupIdMap.size()) {
      // Reached max number of groups that can be assigned to an extension.
      // Discard suggestions from this extension.
      return;
    }
    // This extension doesn't already have an associated groupId. Give it the
    // next available groupId, and give the group the corresponding header for
    // the extension. If the max number of extensions have been assigned a
    // header, don't assign headers to further extensions.
    const omnibox::GroupId current_group_id =
        kReservedGroupIdMap.at(next_available_group_index_++);
    extension_id_to_group_id_map_[extension_id] = current_group_id;

    CHECK_LT(next_available_section_index_, kReservedSectionMap.size());
    const omnibox::GroupSection current_section =
        kReservedSectionMap.at(next_available_section_index_++);

    omnibox::GroupConfig group;
    group.set_section(current_section);
    group.set_render_type(omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL);
    group.set_header_text(base::UTF16ToUTF8(template_url->keyword()));
    provider_->AddToSuggestionGroupsMap(current_group_id, std::move(group));
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

void UnscopedExtensionProviderDelegateImpl::OnOmniboxInputEntered() {
  // Input has been accepted, clear the current list of suggestions and ensure
  // any suggestions that may be incoming later with a stale request ID are
  // discarded.
  Stop(/*clear_cached_results=*/true);
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

  TemplateURLService* turl_service = provider_->GetTemplateURLService();
  const TemplateURL* template_url = turl_service->FindTemplateURLForExtension(
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
  match.suggestion_group_id = extension_id_to_group_id_map_[extension_id];
  return match;
}

void UnscopedExtensionProviderDelegateImpl::ClearSuggestions() {
  extension_suggest_matches_.clear();
  extension_id_to_group_id_map_.clear();
  next_available_group_index_ = 0;
  next_available_section_index_ = 0;
}
