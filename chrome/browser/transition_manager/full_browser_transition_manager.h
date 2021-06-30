// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSITION_MANAGER_FULL_BROWSER_TRANSITION_MANAGER_H_
#define CHROME_BROWSER_TRANSITION_MANAGER_FULL_BROWSER_TRANSITION_MANAGER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/simple_factory_key.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class Profile;

// FullBrowserTransitionManager allows SimpleKeyedServices to hook into the
// transition from service manager mode to full browser to complete their
// initialization. This class can only be called on the UI thread (or its
// precursor in service manager mode).
class FullBrowserTransitionManager {
 public:
  using OnProfileCreationCallback = base::OnceCallback<void(Profile*)>;

  // Return the singleton instance of the class
  static FullBrowserTransitionManager* Get();

  // Register a |callback| to be called on profile creation. If a profile
  // matching the |key| has already been created (i.e. full browser has been
  // loaded), the |callback| is run immediately and this method returns true.
  // Otherwise, it returns false and will be run when/if the profile is created.
  bool RegisterCallbackOnProfileCreation(SimpleFactoryKey* key,
                                         OnProfileCreationCallback callback);

  // Marks that a |profile| has been created.
  void OnProfileCreated(Profile* profile);

  // Marks that a |profile| has beed destroyed.
  void OnProfileDestroyed(Profile* profile);

 private:
  friend class base::NoDestructor<FullBrowserTransitionManager>;

  FullBrowserTransitionManager();
  ~FullBrowserTransitionManager();

  std::map<SimpleFactoryKey*, Profile*> simple_key_to_profile_;
  std::map<SimpleFactoryKey*, std::vector<OnProfileCreationCallback>>
      on_profile_creation_callbacks_;
  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(FullBrowserTransitionManager);
};

#endif  // CHROME_BROWSER_TRANSITION_MANAGER_FULL_BROWSER_TRANSITION_MANAGER_H_
