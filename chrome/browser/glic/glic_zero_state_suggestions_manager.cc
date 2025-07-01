// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace glic {

GlicZeroStateSuggestionsManager::GlicZeroStateSuggestionsManager(
    GlicSharingManager* sharing_manager,
    contextual_cueing::ContextualCueingService* contextual_cueing_service,
    Host* host)
    : sharing_manager_(sharing_manager),
      host_(host),
      contextual_cueing_service_(contextual_cueing_service) {}

GlicZeroStateSuggestionsManager::~GlicZeroStateSuggestionsManager() = default;

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnFocusedTabChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const FocusedTabData& focused_tab_data) {
  content::WebContents* active_web_contents = nullptr;
  if (focused_tab_data.GetFocus().has_value()) {
    active_web_contents = focused_tab_data.GetFocus().value()->GetContents();
  }

  if (contextual_cueing_service_ && active_web_contents) {
    contextual_cueing_service_
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            active_web_contents, is_first_run, supported_tools,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&GlicZeroStateSuggestionsManager::
                                   OnZeroStateSuggestionsNotify,
                               GetWeakPtr(), is_first_run, supported_tools),
                std::nullopt));
  }
}

void GlicZeroStateSuggestionsManager::ObserveZeroStateSuggestions(
    bool is_notifying,
    bool is_first_run,
    const std::vector<std::string>& supported_tools,
    glic::mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
        callback) {
  // Subscribe to changes in the focus tab.
  if (is_notifying) {
    if (current_zero_state_suggestions_focus_change_subscription_) {
      LOG(WARNING) << "Multiple active ZeroStateSuggestion requests";
    }
    // If there was a previous subscription it will be unsubscribed when the
    // old value is destructed on assignment.
    current_zero_state_suggestions_focus_change_subscription_ =
        sharing_manager_->AddFocusedTabChangedCallback(base::BindRepeating(
            &GlicZeroStateSuggestionsManager::
                NotifyZeroStateSuggestionsOnFocusedTabChanged,
            GetWeakPtr(), is_first_run, supported_tools));

    auto* active_web_contents =
        sharing_manager_->GetFocusedTabData().focus()
            ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
            : nullptr;

    if (contextual_cueing_service_ && active_web_contents) {
      contextual_cueing_service_
          ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
              active_web_contents, is_first_run, supported_tools,
              mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                  base::BindOnce(&GlicZeroStateSuggestionsManager::
                                     OnZeroStateSuggestionsFetched,
                                 GetWeakPtr(), std::move(callback)),
                  std::nullopt));
      return;
    }

  } else {
    current_zero_state_suggestions_focus_change_subscription_ = {};
  }

  std::move(callback).Run(nullptr);
}

void GlicZeroStateSuggestionsManager::OnZeroStateSuggestionsFetched(
    mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
        callback,
    std::optional<std::vector<std::string>> returned_suggestions) {
  auto suggestions = mojom::ZeroStateSuggestionsV2::New();
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  if (returned_suggestions) {
    for (const std::string& suggestion_string : returned_suggestions.value()) {
      output_suggestions.push_back(
          mojom::SuggestionContent::New(suggestion_string));
    }
    suggestions->suggestions = std::move(output_suggestions);
  }

  std::move(callback).Run(std::move(suggestions));
}

void GlicZeroStateSuggestionsManager::OnZeroStateSuggestionsNotify(
    bool is_first_run,
    const std::vector<std::string>& supported_tools,
    std::optional<std::vector<std::string>> returned_suggestions) {
  host_->NotifyZeroStateSuggestion(
      returned_suggestions,
      mojom::ZeroStateSuggestionsOptions(is_first_run, supported_tools));
}

void GlicZeroStateSuggestionsManager::Reset() {
  current_zero_state_suggestions_focus_change_subscription_ = {};
}

base::WeakPtr<GlicZeroStateSuggestionsManager>
GlicZeroStateSuggestionsManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
