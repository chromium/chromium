// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "chrome/browser/compose/compose_enabling.h"

#include "base/check.h"
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

bool ComposeEnabling::enabled_for_testing_{false};
bool ComposeEnabling::skip_user_check_for_testing_{false};

ComposeEnabling::ComposeEnabling(
    TranslateLanguageProvider* translate_language_provider,
    Profile* profile,
    signin::IdentityManager* identity_manager,
    OptimizationGuideKeyedService* opt_guide)
    : optimization_guide::SettingsEnabledObserver(
          optimization_guide::proto::MODEL_EXECUTION_FEATURE_COMPOSE),
      profile_(profile),
      opt_guide_(opt_guide),
      identity_manager_(identity_manager) {
  DCHECK(profile_);
  translate_language_provider_ = translate_language_provider;
  if (opt_guide_) {
    // TODO(b/314199871): Add test when this call becomes mock-able.
    opt_guide_->AddModelExecutionSettingsEnabledObserver(this);
  } else {
    LOG(WARNING) << "ComposeEnabling not monitoring for settings change. This "
                    "is expected when running unrelated tests.";
  }
}

ComposeEnabling::~ComposeEnabling() {
  if (opt_guide_) {
    opt_guide_->RemoveModelExecutionSettingsEnabledObserver(this);
  }

  opt_guide_ = nullptr;
  identity_manager_ = nullptr;
  translate_language_provider_ = nullptr;
  profile_ = nullptr;
}

// Static.
void ComposeEnabling::SetEnabledForTesting(bool enabled) {
  enabled_for_testing_ = enabled;
}

// Static.
void ComposeEnabling::SkipUserEnabledCheckForTesting(bool skip) {
  skip_user_check_for_testing_ = skip;
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

// Member function public entry point.
base::expected<void, compose::ComposeShowStatus> ComposeEnabling::IsEnabled() {
  return CheckEnabling(profile_, opt_guide_, identity_manager_);
}

// Static public entry point.
bool ComposeEnabling::IsEnabledForProfile(Profile* profile) {
  OptimizationGuideKeyedService* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  return CheckEnabling(profile, opt_guide, identity_manager).has_value();
}

// Internal static.
base::expected<void, compose::ComposeShowStatus> ComposeEnabling::CheckEnabling(
    Profile* profile,
    OptimizationGuideKeyedService* opt_guide,
    signin::IdentityManager* identity_manager) {
  if (enabled_for_testing_) {
    DVLOG(2) << "enabled for testing";
    return base::ok();
  }

  if (profile == nullptr || identity_manager == nullptr) {
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
    return base::unexpected(compose::ComposeShowStatus::kGenericBlocked);
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

  if (!PageLevelChecks(translate_manager, top_level_frame_origin,
                       element_frame_origin)
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
      translate_manager, rfh->GetMainFrame()->GetLastCommittedOrigin(),
      params.frame_origin);
  if (show_status.has_value()) {
    compose::LogComposeContextMenuShowStatus(
        compose::ComposeShowStatus::kShouldShow);
    return true;
  }
  compose::LogComposeContextMenuShowStatus(show_status.error());
  return false;
}

// TODO(b/314327112): add a browser test to confirm correct enabling.
void ComposeEnabling::PrepareToEnableOnRestart() {
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage;
  about_flags::GetStorage(
      profile_, base::BindOnce(
                    [](std::unique_ptr<flags_ui::FlagsStorage>* final_storage,
                       std::unique_ptr<flags_ui::FlagsStorage> storage,
                       flags_ui::FlagAccess access) {
                      CHECK(access == flags_ui::FlagAccess::kOwnerAccessToFlags)
                          << "ChromeOS is not yet supported";
                      *final_storage = std::move(storage);
                    },
                    base::Unretained(&flags_storage)));
  CHECK(flags_storage) << "Flags storage must be set synchronously; ChromeOS "
                          "(Ash) is not yet supported";

  // Enable required features.
  const std::string enabled_suffix =
      std::string({flags_ui::kMultiSeparatorChar, '1'});
  const std::string compose_enabled_name =
      flag_descriptions::kComposeId + enabled_suffix;
  about_flags::SetFeatureEntryEnabled(flags_storage.get(), compose_enabled_name,
                                      true);
  const std::string autofill_ce_enabled_name =
      flag_descriptions::kAutofillContentEditablesId + enabled_suffix;
  about_flags::SetFeatureEntryEnabled(flags_storage.get(),
                                      autofill_ce_enabled_name, true);
}

base::expected<void, compose::ComposeShowStatus>
ComposeEnabling::PageLevelChecks(translate::TranslateManager* translate_manager,
                                 const url::Origin& top_level_frame_origin,
                                 const url::Origin& element_frame_origin) {
  if (auto profile_show_status = IsEnabled();
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

  // TODO(b/316628813): Check that we have enough space in the browser window to
  // show the dialog.

  return base::ok();
}
