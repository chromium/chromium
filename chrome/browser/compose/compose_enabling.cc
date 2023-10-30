// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "chrome/browser/compose/compose_enabling.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
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

bool ComposeEnabling::IsEnabledForProfile(Profile* profile) {
#if BUILDFLAG(ENABLE_COMPOSE)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return IsEnabled(profile, identity_manager);
#else
  return false;
#endif
}

bool ComposeEnabling::IsEnabled(Profile* profile,
                                signin::IdentityManager* identity_manager) {
  if (enabled_for_testing_) {
    return true;
  }

  if (profile == nullptr || identity_manager == nullptr) {
    return false;
  }

  // Check that the feature flag is enabled.
  if (!base::FeatureList::IsEnabled(compose::features::kEnableCompose) &&
      !base::FeatureList::IsEnabled(compose::features::kFillMultiLine)) {
    DVLOG(2) << "feature not enabled ";
    return false;
  }

  // Check MSBB.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs());
  if (helper != nullptr && !helper->IsEnabled()) {
    DVLOG(2) << "MSBB not enabled " << __func__;
    return false;
  }

  // Check signin status.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    DVLOG(2) << "user not signed in " << __func__;
    return false;
  }

  // TODO(b/305245821): Check age.
  // TODO(b/305246349): Check finch (or maybe labs).
  // TODO(b/305245736): Check consent once it is available to check.

  return true;
}

// TODO(b/303502029): make return state an enum instead of a bool so we
// can return a different value when we have saved state for this field.
bool ComposeEnabling::ShouldTriggerPopup(
    std::string_view autocomplete_attribute,
    Profile* profile,
    translate::TranslateManager* translate_manager,
    bool has_saved_state) {
  if (!base::FeatureList::IsEnabled(compose::features::kEnableComposeNudge)) {
    return false;
  }

  if (!PageLevelChecks(profile, translate_manager)) {
    return false;
  }

  // Check autocomplete attribute.
  if (!AutocompleteAllowed(autocomplete_attribute)) {
    DVLOG(2) << "autocomplete=off";
    return false;
  }

  if (has_saved_state) {
    DVLOG(2) << "has saved state";
    return false;
  }

  // TODO(b/301609046): Add ContentEditable and TextArea checks.

  return true;
}

bool ComposeEnabling::ShouldTriggerContextMenu(
    Profile* profile,
    translate::TranslateManager* translate_manager,
    content::RenderFrameHost* rfh,
    content::ContextMenuParams& params) {
  if (!(params.is_content_editable_for_autofill ||
        (params.form_control_type &&
         *params.form_control_type ==
             blink::mojom::FormControlType::kTextArea))) {
    return false;
  }
  return PageLevelChecks(profile, translate_manager);
}

bool ComposeEnabling::PageLevelChecks(
    Profile* profile,
    translate::TranslateManager* translate_manager) {
  if (!IsEnabledForProfile(profile)) {
    DVLOG(2) << "not enabled";
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          compose::features::kEnableComposeLanguageBypass) &&
      !translate_language_provider_->IsLanguageSupported(translate_manager)) {
    DVLOG(2) << "language not supported";
    return false;
  }

  // TODO(b/301609046): Check with the optimization guide.
  // TODO(b/301609046): Check that we have enough space to show the dialog.

  return true;
}
