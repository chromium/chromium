// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_SIGNALS_DISCLAIMER_METRICS_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_SIGNALS_DISCLAIMER_METRICS_H_

inline constexpr char kEnterpriseSignalsDisclaimerModalShown[] =
    "Enterprise.SignalsDisclaimer.Modal.Shown";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(EnterpriseSignalsDisclaimerModalResult)
enum class EnterpriseSignalsDisclaimerModalResult {
  kAccepted = 0,
  kDeclined,
  kDismissedByAnotherWindow,
  kDismissedWithoutExplicitUserAction,
  kMaxValue = kDismissedWithoutExplicitUserAction
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSignalsDisclaimerModalResult)

inline constexpr char kEnterpriseSignalsDisclaimerModalResult[] =
    "Enterprise.SignalsDisclaimer.Modal.Result";

inline constexpr char kEnterpriseSignalsDisclaimerModalLearnMoreClicked[] =
    "Enterprise.SignalsDisclaimer.Modal.LearnMoreClicked";

inline constexpr char kEnterpriseSignalsDisclaimerProfilePickerShown[] =
    "Enterprise.SignalsDisclaimer.ProfilePicker.Shown";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(EnterpriseSignalsDisclaimerProfilePickerResult)
enum class EnterpriseSignalsDisclaimerProfilePickerResult {
  kAccepted = 0,
  kDeclined,
  kProfilePickerClosed,
  kDismissedWithoutExplicitUserAction,
  kMaxValue = kDismissedWithoutExplicitUserAction
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSignalsDisclaimerProfilePickerResult)

inline constexpr char kEnterpriseSignalsDisclaimerProfilePickerResult[] =
    "Enterprise.SignalsDisclaimer.ProfilePicker.Result";

inline constexpr char
    kEnterpriseSignalsDisclaimerProfilePickerLearnMoreClicked[] =
        "Enterprise.SignalsDisclaimer.ProfilePicker.LearnMoreClicked";

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_SIGNALS_DISCLAIMER_METRICS_H_
