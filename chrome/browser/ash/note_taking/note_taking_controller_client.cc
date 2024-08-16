// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/note_taking/note_taking_controller_client.h"

#include "chrome/browser/ash/profiles/profile_helper.h"

namespace ash {

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
  helper_->LaunchAppForNewNote(profile_);
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
  DCHECK(profile_observation_.IsObservingSource(profile_));
  profile_observation_.Reset();
  profile_ = nullptr;
}

void NoteTakingControllerClient::SetProfileByUser(
    const user_manager::User* user) {
  profile_ = ProfileHelper::Get()->GetProfileByUser(user);
  profile_observation_.Reset();
  profile_observation_.Observe(profile_);
}

}  // namespace ash
