// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "chrome/browser/compose/compose_enabling.h"

#include "chrome/browser/compose/proto/compose_optimization_guide.pb.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "compose_enabling.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"

namespace {

bool AutocompleteAllowed(std::string_view autocomplete_attribute) {
  // Check autocomplete is not turned off.
  return autocomplete_attribute != std::string("off");
}

}  // namespace

ComposeEnabling::ComposeEnabling(
    TranslateLanguageProvider* translate_language_provider) {
  enabled_for_testing_ = false;
  translate_language_provider_ = translate_language_provider;
}

ComposeEnabling::~ComposeEnabling() = default;

void ComposeEnabling::SetEnabledForTesting() {
  enabled_for_testing_ = true;
}

void ComposeEnabling::ClearEnabledForTesting() {
  enabled_for_testing_ = false;
}

void ComposeEnabling::SetOptimizationGuideForTest(
    optimization_guide::OptimizationGuideDecider* opt_guide) {
  opt_guide_ = opt_guide;
}

compose::ComposeHintDecision ComposeEnabling::GetOptimizationGuidanceForUrl(
    const GURL& url,
    Profile* profile) {
  // Use the test value if set, otherwise get from the profile.
  if (!opt_guide_) {
    opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  }

  if (!opt_guide_) {
    DVLOG(2) << "Optimization guide not found, returns unspecified";
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  optimization_guide::OptimizationMetadata metadata;

  auto opt_guide_has_hint = opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::COMPOSE, &metadata);
  if (opt_guide_has_hint !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    DVLOG(2) << "Optimization guide has no hint, returns unspecified";
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  absl::optional<compose::ComposeHintMetadata> compose_metadata =
      optimization_guide::ParsedAnyMetadata<compose::ComposeHintMetadata>(
          metadata.any_metadata().value());
  if (!compose_metadata.has_value()) {
    DVLOG(2) << "Optimization guide has no metadata, returns unspecified";
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  DVLOG(2) << "Optimization guide returns enum "
           << static_cast<int>(compose_metadata->decision());
  return compose_metadata->decision();
}

base::expected<void, compose::ComposeShowStatus>
ComposeEnabling::IsEnabledForProfile(Profile* profile) {
#if BUILDFLAG(ENABLE_COMPOSE)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return IsEnabled(profile, identity_manager);
#else
  return base::unexpected(compose::ComposeShowStatus::kGenericBlocked);
#endif
}

base::expected<void, compose::ComposeShowStatus> ComposeEnabling::IsEnabled(
    Profile* profile,
    signin::IdentityManager* identity_manager) {
  if (enabled_for_testing_) {
    return base::ok();
  }

  if (profile == nullptr || identity_manager == nullptr) {
    return base::unexpected(compose::ComposeShowStatus::kGenericBlocked);
  }

  // Check that the feature flag is enabled.
  if (!base::FeatureList::IsEnabled(compose::features::kEnableCompose)) {
    DVLOG(2) << "feature not enabled ";
    return base::unexpected(compose::ComposeShowStatus::kGenericBlocked);
  }

  // Check MSBB.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs());
  if (helper != nullptr && !helper->IsEnabled()) {
    DVLOG(2) << "MSBB not enabled " << __func__;
    return base::unexpected(compose::ComposeShowStatus::kDisabledMsbb);
  }

  // Check signin status.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty() ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          core_account_info.account_id)) {
    DVLOG(2) << "user not signed in " << __func__;
    return base::unexpected(compose::ComposeShowStatus::kSignedOut);
  }

  // TODO(b/305245821): Check age.
  // TODO(b/305246349): Check finch (or maybe labs).
  // TODO(b/305245736): Check consent once it is available to check.

  return base::ok();
}

// TODO(b/303502029): make return state an enum instead of a bool so we
// can return a different value when we have saved state for this field.
bool ComposeEnabling::ShouldTriggerPopup(
    std::string_view autocomplete_attribute,
    Profile* profile,
    translate::TranslateManager* translate_manager,
    bool ongoing_session,
    const url::Origin& top_level_frame_origin,
    const url::Origin& element_frame_origin,
    GURL url) {
  if (!base::FeatureList::IsEnabled(compose::features::kEnableComposeNudge)) {
    return false;
  }

  // Check URL with Optimization guide.
  compose::ComposeHintDecision decision =
      GetOptimizationGuidanceForUrl(url, profile);
  if (decision == compose::ComposeHintDecision::
                      COMPOSE_HINT_DECISION_COMPOSE_DISABLED ||
      decision ==
          compose::ComposeHintDecision::COMPOSE_HINT_DECISION_DISABLE_NUDGE) {
    return false;
  }

  if (!PageLevelChecks(profile, translate_manager, top_level_frame_origin,
                       element_frame_origin)
           .has_value()) {
    return false;
  }

  // Check autocomplete attribute.
  if (!AutocompleteAllowed(autocomplete_attribute)) {
    DVLOG(2) << "autocomplete=off";
    return false;
  }

  auto& config = compose::GetComposeConfig();

  if (ongoing_session) {
    if (!config.popup_with_saved_state) {
      return false;
    }
  } else {
    if (!config.popup_with_no_saved_state) {
      return false;
    }
  }

  return true;
}

bool ComposeEnabling::ShouldTriggerContextMenu(
    Profile* profile,
    translate::TranslateManager* translate_manager,
    content::RenderFrameHost* rfh,
    content::ContextMenuParams& params) {
  // Make sure the underlying field is one the feature works for.
  if (!(params.is_content_editable_for_autofill ||
        (params.form_control_type &&
         *params.form_control_type ==
             blink::mojom::FormControlType::kTextArea))) {
    compose::LogComposeContextMenuShowStatus(
        compose::ComposeShowStatus::kIncompatibleFieldType);
    return false;
  }

  // Get the page URL of the outermost frame.
  GURL url = rfh->GetMainFrame()->GetLastCommittedURL();

  // Check URL with the optimization guide.
  compose::ComposeHintDecision decision =
      GetOptimizationGuidanceForUrl(url, profile);
  if (decision ==
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_COMPOSE_DISABLED) {
    compose::LogComposeContextMenuShowStatus(
        compose::ComposeShowStatus::kPerUrlChecksFailed);
    return false;
  }

  auto show_status = PageLevelChecks(
      profile, translate_manager, rfh->GetMainFrame()->GetLastCommittedOrigin(),
      params.frame_origin);
  if (show_status.has_value()) {
    compose::LogComposeContextMenuShowStatus(
        compose::ComposeShowStatus::kShouldShow);
    return true;
  }
  compose::LogComposeContextMenuShowStatus(show_status.error());
  return false;
}

base::expected<void, compose::ComposeShowStatus>
ComposeEnabling::PageLevelChecks(Profile* profile,
                                 translate::TranslateManager* translate_manager,
                                 const url::Origin& top_level_frame_origin,
                                 const url::Origin& element_frame_origin) {
  if (auto profile_show_status = IsEnabledForProfile(profile);
      !profile_show_status.has_value()) {
    DVLOG(2) << "not enabled";
    return profile_show_status;
  }

  // Note: This does not check frames between the current and the top level
  // frame. Because all our metadata for compose is either based on the origin
  // of the top level frame or actually part of the top level frame, this is
  // sufficient for now. TODO(b/309162238) follow up on whether this is
  // sufficient long-term.
  if (top_level_frame_origin != element_frame_origin) {
    DVLOG(2) << "cross frame origin not supported";
    return base::unexpected(
        compose::ComposeShowStatus::kFormFieldInCrossOriginFrame);
  }

  if (!base::FeatureList::IsEnabled(
          compose::features::kEnableComposeLanguageBypass) &&
      !translate_language_provider_->IsLanguageSupported(translate_manager)) {
    DVLOG(2) << "language not supported";
    return base::unexpected(compose::ComposeShowStatus::kUnsupportedLanguage);
  }

  // TODO(b/301609046): Check that we have enough space in the browser window to
  // show the dialog.

  return base::ok();
}
