// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_preparation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

// Specify literal defaults per platform: Glic's `common_checks.py` requires an
// explicit `BUILDFLAG` check, while root `PRESUBMIT.py` (`cpp_feature_parser`)
// rejects `FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS` on new features.
BASE_FEATURE(kGlicPinSelectedTabsOnInvoke,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Whether `target.live_mode_behavior` can have any effect on an invocation
// targeting `target.surface`. Apart from `kFail`, these behaviors work by
// choosing between a tab's side panel and the floaty, so they need a surface
// that resolves to one of those up front. `LastActiveOrNew` instead resolves to
// whichever surface the instance is already on, so nothing can steer it.
bool CanLiveModeBehaviorTakeEffect(const Target& target) {
  switch (target.live_mode_behavior) {
    case LiveModeBehavior::kProceedInLiveMode:
    case LiveModeBehavior::kFail:
      return true;
    case LiveModeBehavior::kForceSidePanelTextMode:
      return !std::holds_alternative<Floating>(target.surface) &&
             !std::holds_alternative<LastActiveOrNew>(target.surface);
    case LiveModeBehavior::kForceFloatingTextMode:
      return !std::holds_alternative<LastActiveOrNew>(target.surface);
  }
}

bool IsInlineTrustFirstFreExperimentEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicMessageFirstFre) ||
         base::FeatureList::IsEnabled(features::kGlicActionFirstFRE);
}

// TODO(crbug.com/584742644): Replace with a selected-tabs getter on
// `TabListInterface` (e.g. `GetOrderedMultiSelectedTabs()`) once exposed.
std::vector<tabs::TabInterface*> GetSelectedTabs(TabListInterface& tab_list) {
  std::vector<tabs::TabInterface*> selected_tabs;
  for (tabs::TabInterface* tab : tab_list.GetAllTabs()) {
    if (tab && tab->IsSelected()) {
      selected_tabs.push_back(tab);
    }
  }
  return selected_tabs;
}

// Moves `target_handle` into the last available pin slot if it sits past the
// `sharing_manager` pin limit, preserving the relative order of all other tabs.
void EnsureTargetTabWithinPinLimit(
    const GlicSharingManagerInternal& sharing_manager,
    tabs::TabHandle target_handle,
    std::vector<tabs::TabHandle>& tabs_to_pin) {
  const int32_t available_slots =
      sharing_manager.GetMaxPinnedTabs() - sharing_manager.GetNumPinnedTabs();
  if (available_slots <= 0 ||
      tabs_to_pin.size() <= static_cast<size_t>(available_slots)) {
    return;
  }
  const auto last_slot = tabs_to_pin.begin() + (available_slots - 1);
  const auto target_it = std::ranges::find(tabs_to_pin, target_handle);
  if (target_it > last_slot && target_it != tabs_to_pin.end()) {
    std::ranges::rotate(last_slot, target_it, target_it + 1);
  }
}

}  // namespace

std::optional<GlicInvokeError> ValidateInvokeOptions(
    const GlicInvokeOptions& options) {
  if (const auto* tab_handle =
          std::get_if<tabs::TabHandle>(&options.target.surface)) {
    if (tab_handle->raw_value() == tabs::TabHandle::NullValue) {
      return GlicInvokeError::kInvalidTab;
    }
  }

  if (!CanLiveModeBehaviorTakeEffect(options.target)) {
    return GlicInvokeError::kInvalidConfiguration;
  }

  if (options.RequiresContextClipboardPolicyCheck() &&
      options.RequiresPromptClipboardPolicyCheck()) {
    return GlicInvokeError::kInvalidConfiguration;
  }

  return std::nullopt;
}

base::expected<bool, GlicInvokeError> ResolveFloatyPreservation(
    bool is_showing_live_mode,
    bool is_floating,
    const GlicInvokeOptions& options) {
  if (is_showing_live_mode) {
    switch (options.target.live_mode_behavior) {
      case LiveModeBehavior::kFail:
        return base::unexpected(GlicInvokeError::kLiveModeActive);
      case LiveModeBehavior::kProceedInLiveMode:
        // Live mode runs in the floaty, so carrying on in live mode means
        // leaving the conversation where it is.
        return is_floating;
      case LiveModeBehavior::kForceFloatingTextMode:
        // Unlike the above, this pulls the conversation into the floaty.
        return true;
      case LiveModeBehavior::kForceSidePanelTextMode:
        return false;
    }
  }
  return options.preserve_active_surface && is_floating;
}

bool ShouldUseTrustFirstInlineFre(Profile* profile) {
  return !GlicEnabling::HasConsentedForProfile(profile) &&
         IsInlineTrustFirstFreExperimentEnabled();
}

// TODO(b/571546684): Move all special-case FRE selection into Chrome as
// explicit FreOverrides and remove this exclusion list. Until then, skip
// native FRE for experiments and invocation sources where the web client
// overrides the default FRE while Chrome leaves `fre_override` as
// `kUnspecified`.
bool HasSpecialCaseWebFre(const GlicInvokeOptions& options) {
  if (options.feature_mode.value_or(mojom::FeatureMode::kUnspecified) !=
      mojom::FeatureMode::kUnspecified) {
    return true;
  }
  if (IsInlineTrustFirstFreExperimentEnabled()) {
    return true;
  }
  switch (options.GetInvocationSource()) {
    case mojom::InvocationSource::kNavigationCapture:
    case mojom::InvocationSource::kUniversalCart:
    case mojom::InvocationSource::kPasswordChange:
    case mojom::InvocationSource::kIndigoPageAction:
    case mojom::InvocationSource::kPromotionPage:
      return true;
    default:
      return false;
  }
}

bool CanUseNativeFre(const GlicInvokeOptions& options, Profile* profile) {
  return options.fre_override == mojom::FreOverride::kUnspecified &&
         !HasSpecialCaseWebFre(options) &&
         GlicEnabling::IsProfileEligibleForNativeFre(profile);
}

void MaybePopulateSelectedTabsToPin(
    Profile* profile,
    const GlicSharingManagerInternal& sharing_manager,
    const GlicInvokeHandler::ResolvedTarget& resolved_target,
    bool has_auto_submit_passkey,
    GlicInvokeOptions& options) {
  if (!base::FeatureList::IsEnabled(kGlicPinSelectedTabsOnInvoke)) {
    return;
  }

  if (!options.pin_on_bind || !ShouldPinOnBind(profile) ||
      !options.tab_sharing.tabs_to_pin.empty() ||
      !std::holds_alternative<DefaultConversation>(
          options.target.conversation) ||
      GlicInvokeHandler::RequiresClientInvoke(options,
                                              has_auto_submit_passkey)) {
    return;
  }

  const auto* tab_surface =
      std::get_if<GlicInvokeHandler::TabSurface>(&resolved_target);
  if (!tab_surface || tab_surface->is_new || !tab_surface->tab) {
    return;
  }

  tabs::TabInterface* target_tab = tab_surface->tab;
  BrowserWindowInterface* browser = target_tab->GetBrowserWindowInterface();
  TabListInterface* tab_list =
      browser ? TabListInterface::From(browser) : nullptr;
  if (!tab_list || tab_list->GetActiveTab() != target_tab) {
    return;
  }

  const tabs::TabHandle target_handle = target_tab->GetHandle();
  const std::optional<split_tabs::SplitTabId> target_split =
      target_tab->GetSplit();
  std::vector<tabs::TabHandle> candidate_handles;
  bool saw_target_tab = false;
  bool has_other_selected_tab = false;

  for (tabs::TabInterface* candidate : GetSelectedTabs(*tab_list)) {
    if (candidate == target_tab) {
      saw_target_tab = true;
    } else if (!target_split.has_value() ||
               candidate->GetSplit() != target_split) {
      // `TabStripModel` marks all panes of an active split tab as selected even
      // without multi-selection; only treat the tab strip as multi-selected
      // when at least one selected tab lies outside `target_tab`'s split.
      has_other_selected_tab = true;
    }
    candidate_handles.push_back(candidate->GetHandle());
  }

  if (!has_other_selected_tab) {
    return;
  }

  if (!saw_target_tab) {
    candidate_handles.push_back(target_handle);
  }

  std::erase_if(candidate_handles, [&sharing_manager](tabs::TabHandle handle) {
    return sharing_manager.IsTabPinned(handle);
  });
  if (candidate_handles.empty()) {
    return;
  }

  EnsureTargetTabWithinPinLimit(sharing_manager, target_handle,
                                candidate_handles);

  options.tab_sharing = TabSharingOptions(std::move(candidate_handles),
                                          GlicPinTrigger::kInstanceCreation);
}

}  // namespace glic
