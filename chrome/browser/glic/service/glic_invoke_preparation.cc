// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_preparation.h"

#include <variant>

#include "base/feature_list.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

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

}  // namespace glic
