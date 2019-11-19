// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTE_TAKING_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_NOTE_TAKING_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/note_taking_client.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

class NoteTakingControllerClient
    : public ash::NoteTakingClient,
      public user_manager::UserManager::UserSessionStateObserver,
      public ProfileObserver {
 public:
  explicit NoteTakingControllerClient(NoteTakingHelper* helper);
  ~NoteTakingControllerClient() override;

  // ash::NoteTakingClient:
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
  NoteTakingHelper* helper_;

  // Unowned pointer to the active profile.
  Profile* profile_ = nullptr;
  ScopedObserver<Profile, ProfileObserver> profile_observer_{this};

  base::WeakPtrFactory<NoteTakingControllerClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NoteTakingControllerClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTE_TAKING_CONTROLLER_CLIENT_H_
