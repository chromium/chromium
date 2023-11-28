// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CLIENT_SIDE_TRIAL_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CLIENT_SIDE_TRIAL_H_

#include "base/metrics/field_trial.h"
#include "components/signin/public/base/signin_buildflags.h"

#if !BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#error "Unsupported platform"
#endif

namespace base {
class FeatureList;
}

namespace version_info {
enum class Channel;
}

class PrefService;
class PrefRegistrySimple;

namespace SearchEngineChoiceClientSideTrial {

inline constexpr char kSyntheticTrialName[] = "WaffleSynthetic";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Enrolls the client in a trial and overrides the SearchEngineChoice related
// features according the the selected group.
// Note: Does not perform the synthetic trial registration, it has to be done
// at a later time by calling `RegisterSyntheticTrials()`, as that requires
// `g_browser_process` to be fully initialized, which is typically not the
// case when this method is called.
void SetUpIfNeeded(const base::FieldTrial::EntropyProvider& entropy_provider,
                   base::FeatureList* feature_list,
                   PrefService* local_state);

// Registers a synthetic trial name and group to annotate UMA records based on
// the client-side trial.
// Requires `g_browser_process` to be fully initialized.
void RegisterSyntheticTrials();

// Overrides the client channel value used when choosing in which group to
// assign this client.
using ScopedChannelOverride =
    base::AutoReset<absl::optional<version_info::Channel>>;
ScopedChannelOverride CreateScopedChannelOverrideForTesting(
    version_info::Channel channel);

}  // namespace SearchEngineChoiceClientSideTrial

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CLIENT_SIDE_TRIAL_H_
