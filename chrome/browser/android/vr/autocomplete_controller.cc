// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/autocomplete_controller.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/util.h"

namespace vr {

namespace {
constexpr size_t kMaxNumberOfSuggestions = 4;
constexpr int kSuggestionThrottlingDelayMs = 150;
}  // namespace

AutocompleteController::AutocompleteController(
    const SuggestionCallback& callback)
    : profile_(ProfileManager::GetActiveUserProfile()),
      suggestion_callback_(callback) {
  auto client = std::make_unique<ChromeAutocompleteProviderClient>(profile_);
  client_ = client.get();
  autocomplete_controller_ = std::make_unique<::AutocompleteController>(
      std::move(client), this,
      AutocompleteClassifier::DefaultOmniboxProviders());
}

AutocompleteController::~AutocompleteController() = default;

void AutocompleteController::Start(const AutocompleteRequest& request) {
  metrics::OmniboxEventProto::PageClassification page_classification =
      metrics::OmniboxEventProto::OTHER;

  AutocompleteInput input(request.text, request.cursor_position,
                          page_classification,
                          ChromeAutocompleteSchemeClassifier(profile_));
  input.set_prevent_inline_autocomplete(request.prevent_inline_autocomplete);

  autocomplete_controller_->Start(input);

  last_request_ = request;
}

void AutocompleteController::Stop() {
  autocomplete_controller_->Stop(true);
  suggestion_callback_.Run(std::make_unique<OmniboxSuggestions>());
}

std::tuple<GURL, bool> AutocompleteController::GetUrlFromVoiceInput(
    const base::string16& input) {
  AutocompleteMatch match;
  base::string16 culled_input;
  base::RemoveChars(input, base::ASCIIToUTF16(" "), &culled_input);
  client_->Classify(culled_input, false, false,
                    metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
  if (match.destination_url.is_valid() &&
      (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
       match.type == AutocompleteMatchType::HISTORY_URL ||
       match.type == AutocompleteMatchType::NAVSUGGEST)) {
    return {match.destination_url, true};
  }
  return {GURL(GetDefaultSearchURLForSearchTerms(
              client_->GetTemplateURLService(), input)),
          false};
}

void AutocompleteController::OnResultChanged(bool default_match_changed) {
  auto suggestions = std::make_unique<OmniboxSuggestions>();
  for (const auto& match : autocomplete_controller_->result()) {
    const gfx::VectorIcon* icon = &AutocompleteMatch::TypeToVectorIcon(
        match.type, false, match.document_type);
    suggestions->suggestions.emplace_back(OmniboxSuggestion(
        match.contents, match.description, match.contents_class,
        match.description_class, icon, match.destination_url,
        last_request_.text, match.inline_autocompletion));
    if (suggestions->suggestions.size() >= kMaxNumberOfSuggestions)
      break;
  }
  suggestions_timeout_.Cancel();

  if (suggestions->suggestions.size() < kMaxNumberOfSuggestions) {
    suggestions_timeout_.Reset(
        base::BindRepeating(suggestion_callback_, base::Passed(&suggestions)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, suggestions_timeout_.callback(),
        base::TimeDelta::FromMilliseconds(kSuggestionThrottlingDelayMs));
  } else {
    suggestion_callback_.Run(std::move(suggestions));
  }
}

}  // namespace vr
