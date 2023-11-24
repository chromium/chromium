// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_PROFILE_TAGGER_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_PROFILE_TAGGER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/signin/public/base/signin_buildflags.h"

#if !BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#error Not supported for this platform.
#endif

class ProfileManager;

// Tags new profiles with `prefs::kDefaultSearchProviderChoicePending` so they
// can be later evaluated for choice screen eligibility. This class enables the
// "SearchEngineChoiceTriggerForUntaggedProfiles == false" behaviour, allowing
// to cover new profiles beyond the browser run where they were created.
class SearchEngineChoiceProfileTagger : public ProfileManagerObserver {
 public:
  explicit SearchEngineChoiceProfileTagger(ProfileManager& profile_manager);
  ~SearchEngineChoiceProfileTagger() override;

  // Disallow copy/assign.
  SearchEngineChoiceProfileTagger(const SearchEngineChoiceProfileTagger&) =
      delete;
  SearchEngineChoiceProfileTagger& operator=(
      const SearchEngineChoiceProfileTagger&) = delete;

  // Returns a profile tagger if it is needed based on the current
  // configuration, or a nullptr otherwise.
  static std::unique_ptr<SearchEngineChoiceProfileTagger> Create(
      ProfileManager& profile_manager);

  // ProfileManagerObserver overrides.
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_PROFILE_TAGGER_H_
