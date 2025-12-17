// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/id_type.h"
#include "chrome/browser/contextual_cueing/caching_zero_state_suggestions_manager.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/page_transition_types.h"

namespace glic {

namespace {

BASE_FEATURE(kDisableNewTabZeroStateSuggestions,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCacheZeroStateSuggestions, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRefreshZeroStateSuggestionsOnFocusedTabChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

std::vector<std::string> EmptySuggestions() {
  return {};
}

mojom::ZeroStateSuggestionsV2Ptr MakeEmptySuggestionsPtr() {
  auto suggestions_ptr = mojom::ZeroStateSuggestionsV2::New();
  std::vector<mojom::SuggestionContentPtr> empty_suggestions;
  suggestions_ptr->suggestions = std::move(empty_suggestions);
  return suggestions_ptr;
}

mojom::ZeroStateSuggestionsV2Ptr MakePendingSuggestionsPtr() {
  auto pending_suggestions = MakeEmptySuggestionsPtr();
  pending_suggestions->is_pending = true;
  return pending_suggestions;
}

bool HasUserNavigationBeyondInitial(content::WebContents& web_contents) {
  content::NavigationController& controller = web_contents.GetController();
  int index = controller.GetCurrentEntryIndex();
  // Ignore the initial navigation entry. We're using this function on only
  // new-tab tabs, which have an initial navigation to the new tab page or the
  // user selected default page.
  while (index > 0) {
    content::NavigationEntry* entry = controller.GetEntryAtIndex(index);
    auto transition = base::to_underlying(entry->GetTransitionType());
    bool automatic = 0 != (transition & (ui::PAGE_TRANSITION_FORWARD_BACK |
                                         ui::PAGE_TRANSITION_HOME_PAGE |
                                         ui::PAGE_TRANSITION_FROM_API |
                                         ui::PAGE_TRANSITION_IS_REDIRECT_MASK));
    if (!automatic) {
      return true;
    }
    --index;
  }
  return false;
}

}  // namespace

GlicZeroStateSuggestionsManager::GlicZeroStateSuggestionsManager(
    GlicSharingManager* sharing_manager,
    GlicInstance* glic_instance,
    contextual_cueing::ContextualCueingService* contextual_cueing_service)
    : sharing_manager_(sharing_manager),
      glic_instance_(glic_instance),
      contextual_cueing_service_(contextual_cueing_service) {
  if (contextual_cueing_service &&
      base::FeatureList::IsEnabled(kCacheZeroStateSuggestions)) {
    caching_zero_state_manager_ =
        contextual_cueing::CreateCachingZeroStateSuggestionsManager(
            contextual_cueing_service);
  }
}

GlicZeroStateSuggestionsManager::~GlicZeroStateSuggestionsManager() = default;

Host& GlicZeroStateSuggestionsManager::host() {
  // TODO(refactor): Eventually GlicInstance should own a
  // GlicZeroStateSuggestionsManager, and that GlicInstance's host should be
  // used.
  return glic_instance_->host();
}

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnFocusedTabDataChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const mojom::TabData* focused_tab_data) {
  if (!glic_instance_->IsShowing()) {
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
    host().NotifyZeroStateSuggestion(
        MakePendingSuggestionsPtr(),
        mojom::ZeroStateSuggestionsOptions(is_first_run, supported_tools));

    if (caching_zero_state_manager_) {
      caching_zero_state_manager_
          ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
              active_web_contents, is_first_run, supported_tools,
              base::BindOnce(&GlicZeroStateSuggestionsManager::
                                 OnZeroStateSuggestionsNotify,
                             GetWeakPtr(), is_first_run, supported_tools));
    } else {
      contextual_cueing_service_
          ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
              active_web_contents, is_first_run, supported_tools,
              mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                  base::BindOnce(&GlicZeroStateSuggestionsManager::
                                     OnZeroStateSuggestionsNotify,
                                 GetWeakPtr(), is_first_run, supported_tools),
                  EmptySuggestions()));
    }
  }
}

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnPinnedTabChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const std::vector<content::WebContents*>& pinned_tab_data) {
  if (!glic_instance_->IsShowing()) {
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

  // Also include the focused tab if there is one.
  FocusedTabData focused_tab_data = sharing_manager_->GetFocusedTabData();
  content::WebContents* active_web_contents =
      focused_tab_data.focus() ? focused_tab_data.focus()->GetContents()
                               : nullptr;
  std::vector<content::WebContents*> contents_for_request = pinned_tab_data;
  if (active_web_contents &&
      !Contains(contents_for_request, active_web_contents)) {
    contents_for_request.push_back(active_web_contents);
  }
  FilterTabs(contents_for_request);

  if (contextual_cueing_service_) {
    if (!caching_zero_state_manager_) {
      // Debounce if we already have an outstanding request for the same set.
      std::optional<std::vector<content::WebContents*>>
          outstanding_pinned_tabs_contents =
              contextual_cueing_service_->GetOutstandingPinnedTabsContents();
      if (outstanding_pinned_tabs_contents &&
          outstanding_pinned_tabs_contents->size() ==
              contents_for_request.size() &&
          std::equal(outstanding_pinned_tabs_contents->begin(),
                     outstanding_pinned_tabs_contents->end(),
                     contents_for_request.begin())) {
        return;
      }
    }

    bool suggestions_pending = false;
    if (caching_zero_state_manager_) {
      suggestions_pending =
          caching_zero_state_manager_
              ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
                  contents_for_request, is_first_run, supported_tools,
                  base::FeatureList::IsEnabled(
                      kRefreshZeroStateSuggestionsOnFocusedTabChange)
                      ? active_web_contents
                      : nullptr,
                  base::BindOnce(&GlicZeroStateSuggestionsManager::
                                     OnZeroStateSuggestionsNotify,
                                 GetWeakPtr(), is_first_run, supported_tools));
    } else {
      suggestions_pending =
          contextual_cueing_service_
              ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
                  contents_for_request, is_first_run, supported_tools,
                  base::FeatureList::IsEnabled(
                      kRefreshZeroStateSuggestionsOnFocusedTabChange)
                      ? active_web_contents
                      : nullptr,
                  mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                      base::BindOnce(&GlicZeroStateSuggestionsManager::
                                         OnZeroStateSuggestionsNotify,
                                     GetWeakPtr(), is_first_run,
                                     supported_tools),
                      EmptySuggestions()));
    }

    if (suggestions_pending) {
      // Notify host that suggestions are pending.
      host().NotifyZeroStateSuggestion(
          MakePendingSuggestionsPtr(),
          mojom::ZeroStateSuggestionsOptions(is_first_run, supported_tools));
    }
  }
}

void GlicZeroStateSuggestionsManager::
    NotifyZeroStateSuggestionsOnPinnedTabDataChanged(
        bool is_first_run,
        const std::vector<std::string>& supported_tools,
        const TabDataChange& tab_data_change) {
  TabDataChangeCauseSet eligible_causes = {
      TabDataChangeCause::kSameDocNavigation,
      TabDataChangeCause::kCrossDocNavigation, TabDataChangeCause::kTabChanged};
  if (base::FeatureList::IsEnabled(
          kRefreshZeroStateSuggestionsOnFocusedTabChange)) {
    // Allow for visibility to be a change to refresh suggestions on if focused
    // tab change suggestion refresh is enabled.
    eligible_causes.Put(TabDataChangeCause::kVisibility);
  }
  if (!tab_data_change.causes.HasAny(eligible_causes)) {
    // Not an eligible change cause, do not refresh suggestions.
    return;
  }

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
      // Do nothing
    } else if (auto pinned_tabs = sharing_manager_->GetPinnedTabs();
               !pinned_tabs.empty()) {
      FilterTabs(pinned_tabs);
      if (pinned_tabs.empty()) {
        std::move(callback).Run(MakeEmptySuggestionsPtr());
        return;
      } else if (caching_zero_state_manager_) {
        caching_zero_state_manager_
            ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
                pinned_tabs, is_first_run, supported_tools,
                /* focused_tab=*/nullptr,
                base::BindOnce(&GlicZeroStateSuggestionsManager::
                                   OnZeroStateSuggestionsFetched,
                               GetWeakPtr(), std::move(callback)));
        return;
      } else {
        contextual_cueing_service_
            ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
                pinned_tabs, is_first_run, supported_tools,
                /* focused_tab=*/nullptr,
                mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                    base::BindOnce(&GlicZeroStateSuggestionsManager::
                                       OnZeroStateSuggestionsFetched,
                                   GetWeakPtr(), std::move(callback)),
                    EmptySuggestions()));
        return;
      }
    } else {
      auto* active_web_contents =
          sharing_manager_->GetFocusedTabData().focus()
              ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
              : nullptr;
      if (active_web_contents) {
        if (caching_zero_state_manager_) {
          caching_zero_state_manager_
              ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
                  active_web_contents, is_first_run, supported_tools,
                  base::BindOnce(&GlicZeroStateSuggestionsManager::
                                     OnZeroStateSuggestionsFetched,
                                 GetWeakPtr(), std::move(callback)));
          return;
        } else {
          contextual_cueing_service_
              ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
                  active_web_contents, is_first_run, supported_tools,
                  mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                      base::BindOnce(&GlicZeroStateSuggestionsManager::
                                         OnZeroStateSuggestionsFetched,
                                     GetWeakPtr(), std::move(callback)),
                      EmptySuggestions()));
          return;
        }
      }
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
  host().NotifyZeroStateSuggestion(
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

void GlicZeroStateSuggestionsManager::FilterTabs(
    std::vector<content::WebContents*>& tabs) {
  if (!base::FeatureList::IsEnabled(kDisableNewTabZeroStateSuggestions)) {
    return;
  }

  // We should not use new tabs which have not been explicitly navigated by the
  // user. Remove them here.
  tabs.erase(
      std::remove_if(
          tabs.begin(), tabs.end(),
          [&](content::WebContents* tab_contents) {
            tabs::TabInterface* tab =
                tabs::TabInterface::GetFromContents(tab_contents);
            auto usage = sharing_manager_->GetPinnedTabUsage(tab->GetHandle());
            if (!usage) {
              return false;
            }
            if (usage->pin_event.trigger == GlicPinTrigger::kNewTabDaisyChain) {
              return !HasUserNavigationBeyondInitial(*tab_contents);
            }
            return false;
          }),
      tabs.end());
}

}  // namespace glic
