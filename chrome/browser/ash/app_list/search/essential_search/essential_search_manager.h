// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_ESSENTIAL_SEARCH_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_ESSENTIAL_SEARCH_MANAGER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"

class Profile;

namespace app_list {

// This class is responsible for fetching SOCS cookie and adding it to the user
// cookie jar to make sure that search done through google.com would only use
// essential cookie and data.
// EssentialSearchManager is still WIP.
class EssentialSearchManager : public ash::SessionObserver {
 public:
  explicit EssentialSearchManager(Profile* primary_profile);
  ~EssentialSearchManager() override;

  // Disallow copy and assign.
  EssentialSearchManager(const EssentialSearchManager&) = delete;
  EssentialSearchManager& operator=(const EssentialSearchManager&) = delete;

  // Returns instance of EssentialSearchManager.
  static std::unique_ptr<EssentialSearchManager> Create(
      Profile* primary_profile);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  void FetchSocsCookie();

  // Used to observe the change in session state.
  base::ScopedObservation<ash::SessionController, ash::SessionObserver>
      scoped_observation_{this};

  const raw_ptr<Profile, ExperimentalAsh> primary_profile_;

  base::WeakPtrFactory<EssentialSearchManager> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_ESSENTIAL_SEARCH_MANAGER_H_
