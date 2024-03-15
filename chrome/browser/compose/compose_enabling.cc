// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_enabling.h"

#include <functional>
#include <memory>
#include <type_traits>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/compose/proto/compose_optimization_guide.pb.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_storage.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"

namespace {

bool AutocompleteAllowed(std::string_view autocomplete_attribute) {
  // Check autocomplete is not turned off.
  return autocomplete_attribute != std::string("off");
}

}  // namespace

int ComposeEnabling::enabled_for_testing_{0};
int ComposeEnabling::skip_user_check_for_testing_{0};

ComposeEnabling::ComposeEnabling(
    TranslateLanguageProvider* translate_language_provider,
    Profile* profile,
    signin::IdentityManager* identity_manager,
    OptimizationGuideKeyedService* opt_guide)
    : profile_(profile),
      opt_guide_(opt_guide),
      identity_manager_(identity_manager) {
  DCHECK(profile_);
  translate_language_provider_ = translate_language_provider;
}

ComposeEnabling::~ComposeEnabling() {
  opt_guide_ = nullptr;
  identity_manager_ = nullptr;
  translate_language_provider_ = nullptr;
  profile_ = nullptr;
}

// Static.
ComposeEnabling::ScopedOverride
ComposeEnabling::ScopedEnableComposeForTesting() {
  enabled_for_testing_++;
  return std::make_unique<base::ScopedClosureRunner>(base::BindOnce(
      [](int& enabled_for_testing) {
        enabled_for_testing--;
        DCHECK(enabled_for_testing >= 0);
      },
      std::ref(enabled_for_testing_)));
}

// Static.
ComposeEnabling::ScopedOverride
ComposeEnabling::ScopedSkipUserCheckForTesting() {
  skip_user_check_for_testing_++;
  return std::make_unique<base::ScopedClosureRunner>(base::BindOnce(
      [](int& skip_user_check_for_testing) {
        skip_user_check_for_testing--;
        DCHECK(skip_user_check_for_testing >= 0);
      },
      std::ref(skip_user_check_for_testing_)));
}

compose::ComposeHintDecision ComposeEnabling::GetOptimizationGuidanceForUrl(
    const GURL& url,
    Profile* profile) {

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

  std::optional<compose::ComposeHintMetadata> compose_metadata;
  if (metadata.any_metadata().has_value()) {
    compose_metadata =
        optimization_guide::ParsedAnyMetadata<compose::ComposeHintMetadata>(
            metadata.any_metadata().value());
  }
  if (!compose_metadata.has_value()) {
    DVLOG(2) << "Optimization guide has no metadata, returns unspecified";
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  DVLOG(2) << "Optimization guide returns enum "
           << static_cast<int>(compose_metadata->decision());
  return compose_metadata->decision();
}

// Member function public entry point.
base::expected<void, compose::ComposeShowStatus> ComposeEnabling::IsEnabled() {
  return CheckEnabling(opt_guide_, identity_manager_);
}

// Static public entry point.
bool ComposeEnabling::IsEnabledForProfile(Profile* profile) {
  OptimizationGuideKeyedService* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  return CheckEnabling(opt_guide, identity_manager).has_value();
}

// Private static.
base::expected<void, compose::ComposeShowStatus> ComposeEnabling::CheckEnabling(
    OptimizationGuideKeyedService* opt_guide,
    signin::IdentityManager* identity_manager) {
  if (enabled_for_testing_) {
    DVLOG(2) << "enabled for testing";
    return base::ok();
  }

  if (identity_manager == nullptr || opt_guide == nullptr) {
    DVLOG(2) << "feature not reachable, a required pointer is nullptr";
    return base::unexpected(compose::ComposeShowStatus::kGenericBlocked);
  }

  // Check if the compose feature is still eligible.
  if (!base::FeatureList::IsEnabled(compose::features::kComposeEligible)) {
    DVLOG(2) << "feature not eligible";
    return base::unexpected(compose::ComposeShowStatus::kNotComposeEligible);
  }

  // Check that the feature flag is enabled.
  if (!base::FeatureList::IsEnabled(compose::features::kEnableCompose)) {
    DVLOG(2) << "feature not enabled ";
    return base::unexpected(compose::ComposeShowStatus::kFeatureFlagDisabled);
  }

  // Check signin status.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty() ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          core_account_info.account_id)) {
    DVLOG(2) << "user not signed in";
    return base::unexpected(compose::ComposeShowStatus::kSignedOut);
  }

  // TODO(b/314199871): Remove test bypass once this check becomes mock-able.
  if (!skip_user_check_for_testing_ &&
      !opt_guide->ShouldFeatureBeCurrentlyEnabledForUser(
          optimization_guide::proto::ModelExecutionFeature::
              MODEL_EXECUTION_FEATURE_COMPOSE)) {
    DVLOG(2) << "Feature not available for this user";
    return base::unexpected(
        compose::ComposeShowStatus::kUserNotAllowedByOptimizationGuide);
  }

  DVLOG(2) << "enabled";
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
    GURL url,
    autofill::AutofillSuggestionTriggerSource trigger_source) {
  if (trigger_source ==
          autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus &&
      !base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification)) {
    return false;
  }

  if (trigger_source !=
          autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus &&
      !base::FeatureList::IsEnabled(compose::features::kEnableComposeNudge)) {
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

  // TODO(b/319661274): Support fenced frame checks from the Autofill popup
  // entry point.
  bool is_in_fenced_frame = false;
  if (!PageLevelChecks(translate_manager, url, top_level_frame_origin,
                       element_frame_origin, is_in_fenced_frame)
           .has_value()) {
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
    // Check autocomplete attribute if the proactive nudge would be presented.
    // TODO(b/303288183): Decide if we should keep this check or not.
    if (!AutocompleteAllowed(autocomplete_attribute)) {
      DVLOG(2) << "autocomplete=off";
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
    DVLOG(2) << "not a supported text field";
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
    DVLOG(2) << "disabled for the main frame URL";
    return false;
  }

  auto show_status = PageLevelChecks(
      translate_manager, url, rfh->GetMainFrame()->GetLastCommittedOrigin(),
      params.frame_origin, rfh->IsNestedWithinFencedFrame());
  if (show_status.has_value()) {
    compose::LogComposeContextMenuShowStatus(
        compose::ComposeShowStatus::kShouldShow);
    return true;
  }
  compose::LogComposeContextMenuShowStatus(show_status.error());
  DVLOG(2) << "page level checks failed";
  return false;
}

base::expected<void, compose::ComposeShowStatus>
ComposeEnabling::PageLevelChecks(translate::TranslateManager* translate_manager,
                                 GURL url,
                                 const url::Origin& top_level_frame_origin,
                                 const url::Origin& element_frame_origin,
                                 bool is_nested_within_fenced_frame) {
  if (auto profile_show_status = IsEnabled();
      !profile_show_status.has_value()) {
    DVLOG(2) << "not enabled";
    return profile_show_status;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    DVLOG(2) << "incorrect scheme";
    return base::unexpected(compose::ComposeShowStatus::kIncorrectScheme);
  }

  if (is_nested_within_fenced_frame) {
    DVLOG(2) << "field nested within fenced frame not supported";
    return base::unexpected(
        compose::ComposeShowStatus::kFormFieldNestedInFencedFrame);
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

  return base::ok();
}
