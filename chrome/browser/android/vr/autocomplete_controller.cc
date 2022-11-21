// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/autocomplete_controller.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/search_engines/util.h"

namespace vr {

namespace {
constexpr size_t kMaxNumberOfSuggestions = 4;
constexpr int kSuggestionThrottlingDelayMs = 150;
}  // namespace

AutocompleteController::AutocompleteController(SuggestionCallback callback)
    : profile_(ProfileManager::GetActiveUserProfile()),
      suggestion_callback_(std::move(callback)) {
  auto client = std::make_unique<ChromeAutocompleteProviderClient>(profile_);
  client_ = client.get();

  autocomplete_controller_ = std::make_unique<::AutocompleteController>(
      std::move(client), AutocompleteClassifier::DefaultOmniboxProviders());
  autocomplete_controller_->AddObserver(this);

  OmniboxControllerEmitter* emitter =
      OmniboxControllerEmitter::GetForBrowserContext(profile_);
  if (emitter)
    autocomplete_controller_->AddObserver(emitter);
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
  suggestion_callback_.Run(std::vector<OmniboxSuggestion>{});
}

std::tuple<GURL, bool> AutocompleteController::GetUrlFromVoiceInput(
    const std::u16string& input) {
  AutocompleteMatch match;
  std::u16string culled_input;
  base::RemoveChars(input, u" ", &culled_input);
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

void AutocompleteController::OnResultChanged(
    ::AutocompleteController* controller,
    bool default_match_changed) {
  DCHECK(controller == autocomplete_controller_.get());

  std::vector<OmniboxSuggestion> suggestions;
  for (const auto& match : autocomplete_controller_->result()) {
    const gfx::VectorIcon* icon = &match.GetVectorIcon(false);
    suggestions.emplace_back(match.contents, match.description,
                             match.contents_class, match.description_class,
                             icon, match.destination_url, last_request_.text,
                             match.inline_autocompletion);
    if (suggestions.size() >= kMaxNumberOfSuggestions)
      break;
  }
  suggestions_timeout_.Cancel();

  if (suggestions.size() < kMaxNumberOfSuggestions) {
    suggestions_timeout_.Reset(
        base::BindOnce(suggestion_callback_, std::move(suggestions)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, suggestions_timeout_.callback(),
        base::Milliseconds(kSuggestionThrottlingDelayMs));
  } else {
    suggestion_callback_.Run(std::move(suggestions));
  }
}

}  // namespace vr
