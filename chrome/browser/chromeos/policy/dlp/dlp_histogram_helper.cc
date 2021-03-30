// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace policy {

std::string GetDlpHistogramPrefix() {
  return "Enterprise.Dlp.";
}

void DlpBooleanHistogram(const std::string& suffix, bool value) {
  base::UmaHistogramBoolean(GetDlpHistogramPrefix() + suffix, value);
}

void DlpRestrictionConfiguredHistogram(DlpRulesManager::Restriction value) {
  base::UmaHistogramEnumeration(
      GetDlpHistogramPrefix() + "RestrictionConfigured", value);
}

}  // namespace policy
