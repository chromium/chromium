// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/launcher_search/search_controller_lacros.h"

#include <utility>

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/core/service_access_type.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"

namespace crosapi {

SearchControllerLacros::SearchControllerLacros(int provider_types)
    : profile_(g_browser_process->profile_manager()->GetProfileByPath(
          ProfileManager::GetPrimaryUserProfilePath())) {
  if (!profile_) {
    // TODO(crbug.com/40189614): Log error metrics if the profile is
    // unavailable.
    return;
  }

  profile_observation_.Observe(profile_.get());

  autocomplete_controller_ = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_),
      provider_types, /*is_cros_launcher=*/true);
  autocomplete_controller_->AddObserver(this);

  favicon_cache_ = std::make_unique<FaviconCache>(
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS));
}

SearchControllerLacros::~SearchControllerLacros() = default;

void SearchControllerLacros::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile, profile_);
  DCHECK(profile_observation_.IsObservingSource(profile_.get()));

  // SearchControllerLacros must shut down before the Profile is destroyed,
  // otherwise there will be a use-after-free.
  weak_ptr_factory_.InvalidateWeakPtrs();
  autocomplete_controller_.reset();
  favicon_cache_.reset();
  profile_observation_.Reset();
  profile_ = nullptr;
}

void SearchControllerLacros::RegisterWithAsh() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<mojom::SearchControllerRegistry>()) {
    return;
  }
  service->GetRemote<mojom::SearchControllerRegistry>()
      ->RegisterSearchController(receiver_.BindNewPipeAndPassRemote());
}

void SearchControllerLacros::Search(const std::u16string& query,
                                    SearchCallback callback) {
  if (!autocomplete_controller_) {
    // TODO(crbug.com/40777889): Log error metrics if the autocomplete
    // controller is unavailable.
    if (publisher_.is_bound() && publisher_.is_connected()) {
      publisher_->OnSearchResultsReceived(
          mojom::SearchStatus::kBackendUnavailable, std::nullopt);
    }
    return;
  }

  autocomplete_controller_->Stop(false);
  // If there is an in-flight session, send a cancellation notification.
  if (publisher_.is_bound() && publisher_.is_connected()) {
    publisher_->OnSearchResultsReceived(mojom::SearchStatus::kCancelled,
                                        std::nullopt);
  }

  // Reset the remote and send a new pending receiver to ash.
  publisher_.reset();
  std::move(callback).Run(publisher_.BindNewEndpointAndPassReceiver());

  // Start the search. Results will be returned through the observer interface.
  AutocompleteInput input =
      AutocompleteInput(query, metrics::OmniboxEventProto::CHROMEOS_APP_LIST,
                        ChromeAutocompleteSchemeClassifier(profile_));
  if (input.text().empty())
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  query_ = query;
  input_ = input;
  autocomplete_controller_->Start(input);
}

void SearchControllerLacros::OnResultChanged(AutocompleteController* controller,
                                             bool default_match_changed) {
  DCHECK_EQ(controller, autocomplete_controller_.get());

  std::vector<mojom::SearchResultPtr> results;
  for (AutocompleteMatch match : autocomplete_controller_->result()) {
    // Calculator results are honorary answer results.
    const bool is_answer =
        match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED ||
        match.type == AutocompleteMatchType::CALCULATOR;
    auto result =
        is_answer
            ? CreateAnswerResult(match, autocomplete_controller_.get(), query_,
                                 input_)
            : CreateResult(
                  match, autocomplete_controller_.get(), favicon_cache_.get(),
                  BookmarkModelFactory::GetForBrowserContext(profile_), input_);

    results.push_back(std::move(result));
  }

  const auto status = autocomplete_controller_->done()
                          ? mojom::SearchStatus::kDone
                          : mojom::SearchStatus::kInProgress;
  publisher_->OnSearchResultsReceived(status, std::move(results));
}

}  // namespace crosapi
