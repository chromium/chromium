// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace glic {

namespace {

mojom::ZeroStateSuggestionsV2Ptr MakePendingSuggestionsPtr() {
  auto pending_suggestions = mojom::ZeroStateSuggestionsV2::New();
  pending_suggestions->is_pending = true;

  std::vector<mojom::SuggestionContentPtr> empty_suggestions;
  pending_suggestions->suggestions = std::move(empty_suggestions);
  return pending_suggestions;
}

}  // namespace

GlicZeroStateSuggestionsManager::GlicZeroStateSuggestionsManager(
    GlicSharingManagerImpl* sharing_manager,
    GlicWindowController* window_controller,
    contextual_cueing::ContextualCueingService* contextual_cueing_service,
    Host* host)
    : sharing_manager_(sharing_manager),
      window_controller_(window_controller),
      host_(host),
      contextual_cueing_service_(contextual_cueing_service) {}

GlicZeroStateSuggestionsManager::~GlicZeroStateSuggestionsManager() = default;

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnFocusedTabDataChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const mojom::TabData* focused_tab_data) {
  if (!window_controller_->IsShowing()) {
    return;
  }

  // Pinned tabs are a more intentional sharing choice than focused tab, so
  // don't refresh the suggestions on focus change if there are pinned tabs.
  if (sharing_manager_->GetNumPinnedTabs()) {
    return;
  }

  content::WebContents* active_web_contents =
      sharing_manager_->GetFocusedTabData().focus()
          ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
          : nullptr;

  if (contextual_cueing_service_ && active_web_contents) {
    // Notify host that suggestions are pending.
    host_->NotifyZeroStateSuggestion(
        MakePendingSuggestionsPtr(),
        mojom::ZeroStateSuggestionsOptions(is_first_run, supported_tools));

    contextual_cueing_service_
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            active_web_contents, is_first_run, supported_tools,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&GlicZeroStateSuggestionsManager::
                                   OnZeroStateSuggestionsNotify,
                               GetWeakPtr(), is_first_run, supported_tools),
                /*returned_suggestions=*/
                std::vector<std::string>({})));
  }
}

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnPinnedTabChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const std::vector<content::WebContents*>& pinned_tab_data) {
  if (!window_controller_->IsShowing()) {
    return;
  }

  if (pinned_tab_data.size() >
      static_cast<size_t>(
          contextual_cueing::kMaxPinnedPagesForTriggeringSuggestions.Get())) {
    if (pause_pinned_subscription_updates_) {
      return;
    }
    pause_pinned_subscription_updates_ = true;
  } else {
    pause_pinned_subscription_updates_ = false;
  }

  if (contextual_cueing_service_) {
    // Notify host that suggestions are pending.
    host_->NotifyZeroStateSuggestion(
        MakePendingSuggestionsPtr(),
        mojom::ZeroStateSuggestionsOptions(is_first_run, supported_tools));

    FocusedTabData focused_tab_data = sharing_manager_->GetFocusedTabData();
    content::WebContents* active_web_contents =
        focused_tab_data.focus() ? focused_tab_data.focus()->GetContents()
                                 : nullptr;
    contextual_cueing_service_
        ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
            pinned_tab_data, is_first_run, supported_tools, active_web_contents,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&GlicZeroStateSuggestionsManager::
                                   OnZeroStateSuggestionsNotify,
                               GetWeakPtr(), is_first_run, supported_tools),
                /*returned_suggestions=*/
                std::vector<std::string>({})));
  }
}

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnPinnedTabDataChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const mojom::TabData* data) {
  NotifyZeroStateSuggestionsOnPinnedTabChanged(
      is_first_run, supported_tools, sharing_manager_->GetPinnedTabs());
}

void GlicZeroStateSuggestionsManager::ObserveZeroStateSuggestions(
    bool is_notifying,
    bool is_first_run,
    const std::vector<std::string>& supported_tools,
    glic::mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
        callback) {
  // Subscribe to changes in sharing.
  if (is_notifying) {
    // If there were previous subscriptions they will be unsubscribed when the
    // old values are destructed on assignment.
    // TODO: b/433738020 - Investigate whether we should listen to a different
    // callback.
    current_zero_state_suggestions_focus_change_subscription_ =
        sharing_manager_->AddFocusedTabDataChangedCallback(base::BindRepeating(
            &GlicZeroStateSuggestionsManager::
                NotifyZeroStateSuggestionsOnFocusedTabDataChanged,
            GetWeakPtr(), is_first_run, supported_tools));
    current_zero_state_suggestions_pinned_tab_change_subscription_ =
        sharing_manager_->AddPinnedTabsChangedCallback(base::BindRepeating(
            &GlicZeroStateSuggestionsManager::
                NotifyZeroStateSuggestionsOnPinnedTabChanged,
            GetWeakPtr(), is_first_run, supported_tools));
    current_zero_state_suggestions_pinned_tab_data_change_subscription_ =
        sharing_manager_->AddPinnedTabDataChangedCallback(base::BindRepeating(
            &GlicZeroStateSuggestionsManager::
                NotifyZeroStateSuggestionsOnPinnedTabDataChanged,
            GetWeakPtr(), is_first_run, supported_tools));

    if (!contextual_cueing_service_) {
      return;
    }

    if (auto pinned_tabs = sharing_manager_->GetPinnedTabs();
        !pinned_tabs.empty()) {
      contextual_cueing_service_
          ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
              pinned_tabs, is_first_run, supported_tools,
              /* focused_tab=*/nullptr,
              mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                  base::BindOnce(&GlicZeroStateSuggestionsManager::
                                     OnZeroStateSuggestionsFetched,
                                 GetWeakPtr(), std::move(callback)),
                  /*returned_suggestions=*/std::vector<std::string>({})));
      return;
    }

    auto* active_web_contents =
        sharing_manager_->GetFocusedTabData().focus()
            ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
            : nullptr;
    if (active_web_contents) {
      contextual_cueing_service_
          ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
              active_web_contents, is_first_run, supported_tools,
              mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                  base::BindOnce(&GlicZeroStateSuggestionsManager::
                                     OnZeroStateSuggestionsFetched,
                                 GetWeakPtr(), std::move(callback)),
                  /*returned_suggestions=*/std::vector<std::string>({})));
      return;
    }
  } else {
    // If is_notifying is false we need to reset the subscriptions.
    Reset();
  }

  std::move(callback).Run(nullptr);
}

void GlicZeroStateSuggestionsManager::OnZeroStateSuggestionsFetched(
    mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
        callback,
    std::vector<std::string> returned_suggestions) {
  auto suggestions = mojom::ZeroStateSuggestionsV2::New();
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  for (const std::string& suggestion_string : returned_suggestions) {
    output_suggestions.push_back(
        mojom::SuggestionContent::New(suggestion_string));
  }
  suggestions->suggestions = std::move(output_suggestions);
  suggestions->is_pending = false;

  std::move(callback).Run(std::move(suggestions));
}

void GlicZeroStateSuggestionsManager::OnZeroStateSuggestionsNotify(
    bool is_first_run,
    const std::vector<std::string>& supported_tools,
    std::vector<std::string> returned_suggestions) {
  auto suggestions_v2 = mojom::ZeroStateSuggestionsV2::New();
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  for (const std::string& suggestion_string : returned_suggestions) {
    output_suggestions.push_back(
        mojom::SuggestionContent::New(suggestion_string));
  }
  suggestions_v2->suggestions = std::move(output_suggestions);
  suggestions_v2->is_pending = false;
  host_->NotifyZeroStateSuggestion(
      std::move(suggestions_v2),
      mojom::ZeroStateSuggestionsOptions(is_first_run, supported_tools));
}

void GlicZeroStateSuggestionsManager::Reset() {
  current_zero_state_suggestions_focus_change_subscription_ = {};
  current_zero_state_suggestions_pinned_tab_change_subscription_ = {};
  current_zero_state_suggestions_pinned_tab_data_change_subscription_ = {};
}

base::WeakPtr<GlicZeroStateSuggestionsManager>
GlicZeroStateSuggestionsManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
