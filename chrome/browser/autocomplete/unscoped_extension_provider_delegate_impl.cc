// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/unscoped_extension_provider_delegate_impl.h"

#include <cstddef>
#include <memory>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/omnibox/omnibox_input_watcher_factory.h"
#include "chrome/browser/omnibox/omnibox_suggestions_watcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/actions/omnibox_extension_action.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/unscoped_extension_provider.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "extensions/browser/extension_util.h"

namespace {
// Max number of unscoped extension suggestions to send per extension.
// LINT.IfChange
constexpr size_t kMaxSuggestionsPerExtension = 4;
// LINT.ThenChange(//components/omnibox/browser/autocomplete_grouper_sections.cc)

// Unscoped Extension suggestions are grouped after all other suggestions. But
// they still need to score within top N suggestions to be shown.
constexpr int kUnscopedExtensionRelevance = 2000;

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
  first_suggestion_relevance_ =
      input.IsZeroSuggest() ? omnibox::kUnscopedExtensionZeroSuggestRelevance
                            : kUnscopedExtensionRelevance;

  for (const std::string& extension_id : unscoped_mode_extension_ids) {
    if (!IsEnabledExtension(extension_id)) {
      continue;
    }

    provider_->set_done(false);
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
  provider_->set_done(true);
}

void UnscopedExtensionProviderDelegateImpl::DeleteSuggestion(
    const TemplateURL* template_url,
    const std::u16string& suggestion_text) {
  if (!IsEnabledExtension(template_url->GetExtensionId())) {
    return;
  }

  extensions::ExtensionOmniboxEventRouter::OnDeleteSuggestion(
      profile_, template_url->GetExtensionId(),
      base::UTF16ToUTF8(suggestion_text));
}

void UnscopedExtensionProviderDelegateImpl::OnOmniboxSuggestionsReady(
    const std::vector<ExtensionSuggestion>& suggestions,
    const int request_id,
    const std::string& extension_id) {
  // Discard suggestions
  // 1) with a stale request ID's.
  // 2) that come from an extension that has already returned suggestions.
  // 3) if the provider is done. since this provider allows post done updates,
  //    it will only be done if the user closes the omnibox, arrows down in the
  //    omnibox, or if all extensions have returned suggestions.
  if (request_id != current_request_id_ ||
      base::Contains(extension_id_to_group_id_map_, extension_id) ||
      provider_->done() || suggestions.empty()) {
    return;
  }

  TemplateURLService* turl_service = provider_->GetTemplateURLService();
  const TemplateURL* template_url = turl_service->FindTemplateURLForExtension(
      extension_id, TemplateURL::OMNIBOX_API_EXTENSION);

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

  for (const auto& suggestion : suggestions) {
    CHECK_GE(first_suggestion_relevance_, 0);
    extension_suggest_matches_.push_back(CreateAutocompleteMatch(
        suggestion, first_suggestion_relevance_--, extension_id));
  }

  ACMatches* matches = provider_->matches();
  // If the number of suggestions already sent from the extension is greater
  // than the allowed limit, only show the first `kMaxSuggestionsPerExtension`
  // suggestions .
  matches->insert(matches->end(), extension_suggest_matches_.begin(),
                  std::min(extension_suggest_matches_.end(),
                           extension_suggest_matches_.begin() +
                               kMaxSuggestionsPerExtension));
  // The only case where done can be be true is when all extensions have
  // returned suggestions.
  if (next_available_group_index_ == kReservedGroupIdMap.size() ||
      provider_->GetTemplateURLService()
              ->GetUnscopedModeExtensionIds()
              .size() == 1) {
    provider_->set_done(true);
  }
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
    const ExtensionSuggestion& suggestion,
    int relevance,
    const std::string& extension_id) {
  AutocompleteMatch match(provider_.get(), relevance, suggestion.deletable,
                          AutocompleteMatchType::SEARCH_OTHER_ENGINE);
  std::u16string trimmed_suggestion_content;
  // Prevents DCHECK in `SplitKeywordFromInput` in AutocompleteInput which
  // assumes leading whitespace is trimmed.
  base::TrimWhitespace(base::UTF8ToUTF16(suggestion.content),
                       base::TRIM_LEADING, &trimmed_suggestion_content);
  match.fill_into_edit = trimmed_suggestion_content;
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

  // No match should have empty classifications.
  CHECK(!suggestion.match_classifications.empty());
  match.contents_class = suggestion.match_classifications;
  match.suggestion_group_id = extension_id_to_group_id_map_[extension_id];

  if (suggestion.actions) {
    for (const auto& action : *suggestion.actions) {
      match.actions.push_back(base::MakeRefCounted<OmniboxExtensionAction>(
          base::UTF8ToUTF16(action.label),
          base::UTF8ToUTF16(action.tooltip_text),
          base::BindRepeating(
              &UnscopedExtensionProviderDelegateImpl::OnActionExecuted,
              weak_factory_.GetWeakPtr(), extension_id, action.name,
              suggestion.content),
          action.icon));
    }
  }

  if (suggestion.icon_url.has_value()) {
    GURL icon_url = GURL(suggestion.icon_url.value());
    match.image_url = icon_url.is_valid() ? icon_url : GURL();
  }

  return match;
}

void UnscopedExtensionProviderDelegateImpl::ClearSuggestions() {
  extension_suggest_matches_.clear();
  extension_id_to_group_id_map_.clear();
  next_available_group_index_ = 0;
  next_available_section_index_ = 0;
}

void UnscopedExtensionProviderDelegateImpl::OnActionExecuted(
    const std::string& extension_id,
    const std::string& action_name,
    const std::string& contents) {
  if (!IsEnabledExtension(extension_id)) {
    return;
  }

  extensions::ExtensionOmniboxEventRouter::OnActionExecuted(
      profile_.get(), extension_id, action_name, contents);
  // Action has been executed, clear the current list of suggestions and ensure
  // any suggestions that may be incoming later with a stale request ID are
  // discarded.
  Stop(/*clear_cached_results=*/true);
}

bool UnscopedExtensionProviderDelegateImpl::IsEnabledExtension(
    const std::string& extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions()
          .GetByID(extension_id);
  return extension;
}
