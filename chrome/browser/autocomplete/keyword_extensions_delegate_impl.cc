// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/omnibox/omnibox_input_watcher_factory.h"
#include "chrome/browser/omnibox/omnibox_suggestions_watcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"

namespace omnibox_api = extensions::api::omnibox;

int KeywordExtensionsDelegateImpl::global_input_uid_ = 0;

KeywordExtensionsDelegateImpl::KeywordExtensionsDelegateImpl(
    Profile* profile,
    KeywordProvider* provider)
    : KeywordExtensionsDelegate(provider),
      profile_(profile),
      provider_(provider) {
  DCHECK(provider_);

  current_input_id_ = 0;

  omnibox_input_observation_.Observe(
      OmniboxInputWatcherFactory::GetForBrowserContext(profile_));

  // TODO(crbug.com/40810217): The comment below is historic and maybe
  // misleading because extensions don't always "run" in the original profile.
  // Review and update as needed.
  //
  // Extension suggestions always come from the original profile, since that's
  // where extensions run. We use the input ID to distinguish whether the
  // suggestions are meant for us.
  omnibox_suggestions_observation_.Observe(
      OmniboxSuggestionsWatcherFactory::GetForBrowserContext(
          profile_->GetOriginalProfile()));
}

KeywordExtensionsDelegateImpl::~KeywordExtensionsDelegateImpl() = default;

void KeywordExtensionsDelegateImpl::DeleteSuggestion(
    const TemplateURL* template_url,
    const std::u16string& suggestion_text) {
  extensions::ExtensionOmniboxEventRouter::OnDeleteSuggestion(
      profile_, template_url->GetExtensionId(),
      base::UTF16ToUTF8(suggestion_text));
}

void KeywordExtensionsDelegateImpl::IncrementInputId() {
  current_input_id_ = ++global_input_uid_;
}

bool KeywordExtensionsDelegateImpl::IsEnabledExtension(
    const std::string& extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions()
          .GetByID(extension_id);
  return extension &&
         (!profile_->IsOffTheRecord() ||
          extensions::util::IsIncognitoEnabled(extension_id, profile_));
}

bool KeywordExtensionsDelegateImpl::Start(
    const AutocompleteInput& input,
    bool minimal_changes,
    const TemplateURL* template_url,
    const std::u16string& remaining_input) {
  DCHECK(template_url);

  bool want_asynchronous_matches = !input.omit_asynchronous_matches();
  if (want_asynchronous_matches) {
    std::string extension_id = template_url->GetExtensionId();
    if (extension_id != current_keyword_extension_id_)
      MaybeEndExtensionKeywordMode();
    if (current_keyword_extension_id_.empty())
      EnterExtensionKeywordMode(extension_id);
  }

  extensions::ApplyDefaultSuggestionForExtensionKeyword(
      profile_, template_url, remaining_input, &matches()->front());

  if (minimal_changes) {
    // If the input hasn't significantly changed, we can just use the
    // suggestions from last time. We need to readjust the relevance to
    // ensure it is less than the main match's relevance.
    for (size_t i = 0; i < extension_suggest_matches_.size(); ++i) {
      matches()->push_back(extension_suggest_matches_[i]);
      matches()->back().relevance = matches()->front().relevance - (i + 1);
    }
  } else if (want_asynchronous_matches) {
    extension_suggest_last_input_ = input;
    extension_suggest_matches_.clear();

    // We only have to wait for suggest results if there are actually
    // extensions listening for input changes.
    if (extensions::ExtensionOmniboxEventRouter::OnInputChanged(
            profile_, template_url->GetExtensionId(),
            base::UTF16ToUTF8(remaining_input), current_input_id_))
      set_done(false);
  }
  return want_asynchronous_matches;
}

void KeywordExtensionsDelegateImpl::EnterExtensionKeywordMode(
    const std::string& extension_id) {
  DCHECK(current_keyword_extension_id_.empty());
  current_keyword_extension_id_ = extension_id;

  extensions::ExtensionOmniboxEventRouter::OnInputStarted(
      profile_, current_keyword_extension_id_);
}

void KeywordExtensionsDelegateImpl::MaybeEndExtensionKeywordMode() {
  if (!current_keyword_extension_id_.empty()) {
    extensions::ExtensionOmniboxEventRouter::OnInputCancelled(
        profile_, current_keyword_extension_id_);
    current_keyword_extension_id_.clear();
    // Ignore stray suggestions_ready events that arrive after
    // OnInputCancelled().
    IncrementInputId();
  }
}

// Input has been accepted, so we're done with this input session. Ensure
// we don't send the OnInputCancelled event, or handle any more stray
// suggestions_ready events.
void KeywordExtensionsDelegateImpl::OnOmniboxInputEntered() {
  current_keyword_extension_id_.clear();
  IncrementInputId();
}

void KeywordExtensionsDelegateImpl::OnOmniboxSuggestionsReady(
    const std::vector<ExtensionSuggestion>& suggestions,
    const int request_id,
    const std::string& extension_id) {
  // Ignore result if it's old or if the provider is done. Checking if the
  // provider is done prevents the extension from sending multiple sets of
  // suggestions for the same request.
  if (request_id != current_input_id_ || provider_->done()) {
    return;
  }

  TemplateURLService* model = provider_->GetTemplateURLService();
  DCHECK(model);

  const AutocompleteInput& input = extension_suggest_last_input_;

  // AutocompleteInput::ExtractKeywordFromInput() can fail if e.g. this code is
  // triggered by direct calls from the development console, outside the normal
  // flow of user input.
  std::u16string keyword, remaining_input;
  if (!AutocompleteInput::ExtractKeywordFromInput(input, model, &keyword,
                                                  &remaining_input)) {
    return;
  }

  const TemplateURL* template_url = model->GetTemplateURLForKeyword(keyword);
  DCHECK_EQ(extension_id, template_url->GetExtensionId());

  // We want to order these suggestions in descending order, so start with
  // the relevance of the first result, added synchronously in
  // KeywordProvider::Start(), and subtract 1 for each subsequent suggestion
  // from the extension. We recompute the first match's relevance; we know that
  // |complete| is true, because we wouldn't get results from the extension
  // unless the full keyword had been typed.
  int first_relevance = KeywordProvider::CalculateRelevance(
      input.type(), /*complete=*/true, /*support_replacement=*/true,
      input.prefer_keyword(), input.allow_exact_keyword_match());

  for (const ExtensionSuggestion& suggestion : suggestions) {
    // Because these matches are async, we should never let them become the
    // default match, lest we introduce race conditions in the omnibox user
    // interaction.
    extension_suggest_matches_.push_back(provider_->CreateAutocompleteMatch(
        template_url, input, keyword.length(),
        base::UTF8ToUTF16(suggestion.content), false, --first_relevance,
        suggestion.deletable));

    AutocompleteMatch* match = &extension_suggest_matches_.back();
    match->contents.assign(base::UTF8ToUTF16(suggestion.description));

    // No match should have empty classifications.
    CHECK(!suggestion.match_classifications.empty());
    match->contents_class = suggestion.match_classifications;
  }

  set_done(true);
  matches()->insert(matches()->end(), extension_suggest_matches_.begin(),
                    extension_suggest_matches_.end());
  OnProviderUpdate(!extension_suggest_matches_.empty());
}

void KeywordExtensionsDelegateImpl::OnOmniboxDefaultSuggestionChanged() {
  TemplateURLService* model = provider_->GetTemplateURLService();
  DCHECK(model);

  const AutocompleteInput& input = extension_suggest_last_input_;

  // It's possible to change the default suggestion while not in an editing
  // session.
  std::u16string keyword, remaining_input;
  if (matches()->empty() || current_keyword_extension_id_.empty() ||
      !AutocompleteInput::ExtractKeywordFromInput(input, model, &keyword,
                                                  &remaining_input)) {
    return;
  }

  const TemplateURL* template_url(model->GetTemplateURLForKeyword(keyword));
  extensions::ApplyDefaultSuggestionForExtensionKeyword(
      profile_, template_url, remaining_input, &matches()->front());
  OnProviderUpdate(true);
}

void KeywordExtensionsDelegateImpl::OnProviderUpdate(bool updated_matches) {
  provider_->NotifyListeners(updated_matches);
}
