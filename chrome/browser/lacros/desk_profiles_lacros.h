// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DESK_PROFILES_LACROS_H_
#define CHROME_BROWSER_LACROS_DESK_PROFILES_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/crosapi/mojom/desk_profiles.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;
class ProfileManager;

namespace crosapi {

class DeskProfilesLacros : public ProfileAttributesStorageObserver,
                           public ProfileManagerObserver {
 public:
  DeskProfilesLacros(ProfileManager* profile_manager,
                     mojom::DeskProfileObserver* remote);
  DeskProfilesLacros(const DeskProfilesLacros&) = delete;
  DeskProfilesLacros& operator=(const DeskProfilesLacros&) = delete;
  ~DeskProfilesLacros() override;

 private:
  // ProfileAttributesStorageObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Sends an upsert of the profile located at `profile_path` to ash.
  void SendProfileUpsert(const base::FilePath& profile_path);

  raw_ptr<ProfileManager> profile_manager_ = nullptr;

  raw_ptr<mojom::DeskProfileObserver> remote_ = nullptr;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      storage_observer_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      manager_observer_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_DESK_PROFILES_LACROS_H_
