// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {
namespace {

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

template <typename FilterFunction, typename... Args>
bool IsConfirmationRequired(ConfirmationType confirmation_type,
                            FilterFunction filter_function,
                            Args&&... args) {
  bool is_confirmation_required =
      privacy_sandbox::kPrivacySandboxSettings4.default_state ==
          base::FEATURE_ENABLED_BY_DEFAULT &&
      std::invoke(filter_function, std::forward<Args>(args)...);

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

bool IsRestrictedNoticeCondition(
    PrivacySandboxCountries* privacy_sandbox_countries) {
  return IsNoticeRequired(privacy_sandbox_countries) ||
         IsConsentRequired(privacy_sandbox_countries);
}

}  // namespace

bool IsConsentRequired(PrivacySandboxCountries* privacy_sandbox_countries) {
  return IsConfirmationRequired(ConfirmationType::Consent,
                                &PrivacySandboxCountries::IsConsentCountry,
                                privacy_sandbox_countries);
}

bool IsNoticeRequired(PrivacySandboxCountries* privacy_sandbox_countries) {
  return IsConfirmationRequired(ConfirmationType::Notice,
                                &PrivacySandboxCountries::IsRestOfWorldCountry,
                                privacy_sandbox_countries);
}

bool IsRestrictedNoticeRequired(
    PrivacySandboxCountries* privacy_sandbox_countries) {
  return IsConfirmationRequired(ConfirmationType::RestrictedNotice,
                                &IsRestrictedNoticeCondition,
                                privacy_sandbox_countries);
}

}  // namespace privacy_sandbox
