// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTE_TAKING_NOTE_TAKING_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_ASH_NOTE_TAKING_NOTE_TAKING_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/note_taking_client.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/user_manager/user_manager.h"

namespace ash {

class NoteTakingControllerClient
    : public NoteTakingClient,
      public user_manager::UserManager::UserSessionStateObserver,
      public ProfileObserver {
 public:
  explicit NoteTakingControllerClient(NoteTakingHelper* helper);

  NoteTakingControllerClient(const NoteTakingControllerClient&) = delete;
  NoteTakingControllerClient& operator=(const NoteTakingControllerClient&) =
      delete;

  ~NoteTakingControllerClient() override;

  // NoteTakingClient:
  bool CanCreateNote() override;
  void CreateNote() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void SetProfileForTesting(Profile* profile) { profile_ = profile; }

 private:
  void SetProfileByUser(const user_manager::User* user);

  // Unowned pointer to the note taking helper.
  raw_ptr<NoteTakingHelper> helper_ = nullptr;

  // Unowned pointer to the active profile.
  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<NoteTakingControllerClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTE_TAKING_NOTE_TAKING_CONTROLLER_CLIENT_H_
