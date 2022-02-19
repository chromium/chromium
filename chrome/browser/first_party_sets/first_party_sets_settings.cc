// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_settings.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

bool IsFirstPartySetsEnabledInternal() {
  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return false;
  }
  if (!g_browser_process) {
    // If browser process doesn't exist (e.g. in minimal mode on android),
    // default to the feature value which is true since we didn't return above.
    return true;
  }
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state ||
      !local_state->FindPreference(first_party_sets::kFirstPartySetsEnabled)) {
    return true;
  }
  return local_state->GetBoolean(first_party_sets::kFirstPartySetsEnabled);
}

}  // namespace

// static
FirstPartySetsSettings* FirstPartySetsSettings::Get() {
  static base::NoDestructor<FirstPartySetsSettings> instance;
  return instance.get();
}

bool FirstPartySetsSettings::IsFirstPartySetsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This method invokes `IsFirstPartySetsEnabledInternal` and uses the
  // `enabled_` variable to memoize the result. We can memoize since the
  // First-Party Sets enterprise policy doesn't support `dynamic refresh` and
  // the base::Feature doesn't change after start up, the value of this method
  // will not change in a single browser session.
  if (!enabled_.has_value()) {
    enabled_ = absl::make_optional(IsFirstPartySetsEnabledInternal());
  }
  return enabled_.value();
}

void FirstPartySetsSettings::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = absl::nullopt;
}
