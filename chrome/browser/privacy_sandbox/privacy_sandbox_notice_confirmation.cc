// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/variations/service/variations_service.h"

namespace privacy_sandbox {

namespace {

constexpr auto kConsentCountries = base::MakeFixedFlatSet<std::string_view>({
    "gb", "at", "ax", "be", "bg", "bl", "ch", "cy", "cz", "de", "dk",
    "ee", "es", "fi", "fr", "gf", "gg", "gi", "gp", "gr", "hr", "hu",
    "ie", "is", "it", "je", "ke", "li", "lt", "lu", "lv", "mf", "mt",
    "mq", "nc", "nl", "no", "pf", "pl", "pm", "pt", "qa", "re", "ro",
    "se", "si", "sk", "sj", "tf", "va", "wf", "yt",
});

enum class ConfirmationType { Notice, Consent, RestrictedNotice };

bool IsFeatureParamEnabled(ConfirmationType confirmation_type) {
  switch (confirmation_type) {
    case ConfirmationType::Notice:
      return privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.Get();
    case ConfirmationType::Consent:
      return privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get();
    case ConfirmationType::RestrictedNotice:
      return privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.Get();
  }
}

void EmitHistogram(ConfirmationType confirmation_type, bool value) {
  switch (confirmation_type) {
    case ConfirmationType::Notice:
      return base::UmaHistogramBoolean(
          "Settings.PrivacySandbox.NoticeCheckIsMismatched", value);
    case ConfirmationType::Consent:
      return base::UmaHistogramBoolean(
          "Settings.PrivacySandbox.ConsentCheckIsMismatched", value);
    case ConfirmationType::RestrictedNotice:
      return base::UmaHistogramBoolean(
          "Settings.PrivacySandbox.RestrictedNoticeCheckIsMismatched", value);
  }
}

template <typename FilterFunction>
bool IsConfirmationRequired(ConfirmationType confirmation_type,
                            FilterFunction filter_function) {
  bool is_confirmation_required =
      privacy_sandbox::kPrivacySandboxSettings4.default_state ==
          base::FEATURE_ENABLED_BY_DEFAULT &&
      filter_function();

  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          privacy_sandbox::kPrivacySandboxSettings4.name)) {
    bool is_confirmation_required_override =
        IsFeatureParamEnabled(confirmation_type);
    EmitHistogram(confirmation_type, is_confirmation_required !=
                                         is_confirmation_required_override);
    return is_confirmation_required_override;
  }
  return is_confirmation_required;
}

}  // namespace

bool IsConsentRequired() {
  CHECK(g_browser_process);
  return IsConfirmationRequired(ConfirmationType::Consent, []() {
    return g_browser_process->variations_service() &&
           kConsentCountries.contains(g_browser_process->variations_service()
                                          ->GetStoredPermanentCountry());
  });
}

bool IsNoticeRequired() {
  CHECK(g_browser_process);
  return IsConfirmationRequired(ConfirmationType::Notice, []() {
    return g_browser_process->variations_service() &&
           !g_browser_process->variations_service()
                ->GetStoredPermanentCountry()
                .empty() &&
           !kConsentCountries.contains(g_browser_process->variations_service()
                                           ->GetStoredPermanentCountry());
  });
}

bool IsRestrictedNoticeRequired() {
  return IsConfirmationRequired(ConfirmationType::RestrictedNotice, []() {
    return IsNoticeRequired() || IsConsentRequired();
  });
}

}  // namespace privacy_sandbox
