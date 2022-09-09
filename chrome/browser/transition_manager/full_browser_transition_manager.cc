// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/transition_manager/full_browser_transition_manager.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"

FullBrowserTransitionManager::FullBrowserTransitionManager() = default;
FullBrowserTransitionManager::~FullBrowserTransitionManager() = default;

// static
FullBrowserTransitionManager* FullBrowserTransitionManager::Get() {
  static base::NoDestructor<FullBrowserTransitionManager> instance;
  return instance.get();
}

bool FullBrowserTransitionManager::RegisterCallbackOnProfileCreation(
    SimpleFactoryKey* key,
    OnProfileCreationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iterator = simple_key_to_profile_.find(key);
  if (iterator != simple_key_to_profile_.end()) {
    // Profile has already been created, run the callback now.
    std::move(callback).Run(iterator->second);
    return true;
  } else {
    on_profile_creation_callbacks_[key].push_back(std::move(callback));
    return false;
  }
}

void FullBrowserTransitionManager::OnProfileCreated(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SimpleFactoryKey* key = profile->GetProfileKey();
  DCHECK(!base::Contains(simple_key_to_profile_, key));

  // Register the mapping so that it can be used if deferred callbacks are added
  // later.
  simple_key_to_profile_[key] = profile;
  auto map_iterator = on_profile_creation_callbacks_.find(key);
  if (map_iterator != on_profile_creation_callbacks_.end()) {
    for (OnProfileCreationCallback& callback : map_iterator->second) {
      std::move(callback).Run(profile);
    }
    map_iterator->second.clear();
  }
}

void FullBrowserTransitionManager::OnProfileDestroyed(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SimpleFactoryKey* key = profile->GetProfileKey();
  simple_key_to_profile_.erase(key);
  on_profile_creation_callbacks_.erase(key);
}
