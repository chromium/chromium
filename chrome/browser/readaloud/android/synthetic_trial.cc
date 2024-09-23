// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/synthetic_trial.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace readaloud {
namespace {

inline constexpr char kSeparator[] = "|||";

bool GetFeatureNameFromKey(const std::string& key, std::string_view* out) {
  auto split_key = base::SplitStringPieceUsingSubstr(
      key, kSeparator, base::WhitespaceHandling::KEEP_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (split_key.size() != 2) {
    return false;
  }
  *out = split_key[0];
  return true;
}

}  // namespace

// static
void SyntheticTrial::ClearStalePrefs() {
  PrefService* prefs = g_browser_process->local_state();

  std::vector<std::string> keys_to_clear;
  for (auto [key, trial_name] :
       prefs->GetDict(prefs::kReadAloudSyntheticTrials)) {
    std::string_view feature;
    if (!GetFeatureNameFromKey(key, &feature)) {
      continue;
    }

    base::FieldTrial* trial =
        base::FeatureList::GetInstance()->GetAssociatedFieldTrialByFeatureName(
            feature);
    if (!trial || trial->trial_name() != trial_name) {
      keys_to_clear.emplace_back(key);
    }
  }

  {
    ScopedDictPrefUpdate update(prefs, prefs::kReadAloudSyntheticTrials);
    for (const std::string& key : keys_to_clear) {
      update->Remove(key);
    }
  }
}

// static
std::unique_ptr<SyntheticTrial> SyntheticTrial::Create(
    const std::string& feature_name,
    const std::string& trial_suffix) {
  if (feature_name.find(kSeparator) != std::string::npos ||
      trial_suffix.find(kSeparator) != std::string::npos) {
    return nullptr;
  }

  base::FieldTrial* trial =
      base::FeatureList::GetInstance()->GetAssociatedFieldTrialByFeatureName(
          feature_name);
  if (!trial) {
    return nullptr;
  }

  return std::make_unique<SyntheticTrial>(feature_name, trial_suffix, trial);
}

SyntheticTrial::SyntheticTrial(const std::string& feature_name,
                               const std::string& trial_suffix,
                               base::FieldTrial* base_trial)
    : feature_name_(feature_name),
      suffix_(trial_suffix),
      base_trial_(base_trial) {
  DCHECK(!suffix_.empty());

  // If this synthetic trial was previously active, reactivate now.
  const std::string* stored_base_trial_name =
      prefs()->GetDict(prefs::kReadAloudSyntheticTrials).FindString(pref_key());
  if (stored_base_trial_name &&
      base_trial_->trial_name() == *stored_base_trial_name) {
    Activate();
  }
}

void SyntheticTrial::Activate() {
  if (synthetic_trial_active_) {
    return;
  }

  const std::string synthetic_trial_name =
      base::StrCat({base_trial_->trial_name(), suffix_});

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      synthetic_trial_name, base_trial_->GetGroupNameWithoutActivation(),
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
  synthetic_trial_active_ = true;

  {
    ScopedDictPrefUpdate(prefs(), prefs::kReadAloudSyntheticTrials)
        ->Set(pref_key(), base_trial_->trial_name());
  }
}

std::string SyntheticTrial::pref_key() const {
  return base::StrCat({feature_name_, kSeparator, suffix_});
}

PrefService* SyntheticTrial::prefs() {
  return g_browser_process->local_state();
}

}  // namespace readaloud
