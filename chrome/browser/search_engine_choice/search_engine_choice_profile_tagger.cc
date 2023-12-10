// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_profile_tagger.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"

SearchEngineChoiceProfileTagger::SearchEngineChoiceProfileTagger(
    ProfileManager& profile_manager) {
  profile_manager_observation_.Observe(&profile_manager);

  // Make sure we attempt tagging profiles added earlier. We might have missed
  // prompting them during this run, but we will get them at the next one.
  for (Profile* profile : profile_manager.GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
}

SearchEngineChoiceProfileTagger::~SearchEngineChoiceProfileTagger() = default;

// static
std::unique_ptr<SearchEngineChoiceProfileTagger>
SearchEngineChoiceProfileTagger::Create(ProfileManager& profile_manager) {
  if (!switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get()) {
    return nullptr;
  }

  return std::make_unique<SearchEngineChoiceProfileTagger>(profile_manager);
}

void SearchEngineChoiceProfileTagger::OnProfileAdded(Profile* profile) {
  if (!profile->IsNewProfile()) {
    return;
  }

  DVLOG(1) << "Tagging profile: " << profile->GetBaseName();
  profile->GetPrefs()->SetBoolean(prefs::kDefaultSearchProviderChoicePending,
                                  true);
}

void SearchEngineChoiceProfileTagger::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}
