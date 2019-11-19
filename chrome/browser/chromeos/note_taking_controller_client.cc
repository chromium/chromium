// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_controller_client.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace chromeos {

NoteTakingControllerClient::NoteTakingControllerClient(NoteTakingHelper* helper)
    : helper_(helper) {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

NoteTakingControllerClient::~NoteTakingControllerClient() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

bool NoteTakingControllerClient::CanCreateNote() {
  return profile_ && helper_->IsAppAvailable(profile_);
}

void NoteTakingControllerClient::CreateNote() {
  helper_->LaunchAppForNewNote(profile_, base::FilePath());
}

void NoteTakingControllerClient::ActiveUserChanged(
    user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&NoteTakingControllerClient::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

void NoteTakingControllerClient::OnProfileWillBeDestroyed(Profile* profile) {
  // Update |profile_| when exiting a session or shutting down.
  DCHECK_EQ(profile_, profile);
  profile_observer_.Remove(profile_);
  profile_ = nullptr;
}

void NoteTakingControllerClient::SetProfileByUser(
    const user_manager::User* user) {
  profile_ = ProfileHelper::Get()->GetProfileByUser(user);
  profile_observer_.RemoveAll();
  profile_observer_.Add(profile_);
}

}  // namespace chromeos
