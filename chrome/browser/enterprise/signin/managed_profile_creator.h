// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_CREATOR_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_CREATOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

using ProfileCreationCallback =
    base::OnceCallback<void(base::WeakPtr<Profile>)>;

// Delegate class for ManagedProfileCreator to correctly set up the profiles it
// creates.
class ManagedProfileCreationDelegate {
 public:
  virtual ~ManagedProfileCreationDelegate() = default;

  // Sets profile attributes related to management.
  virtual void SetManagedAttributesForProfile(
      ProfileAttributesEntry* entry) = 0;

  // Performs any checks that are needed to ensure the new profile is set up
  // correctly.
  virtual void CheckManagedProfileStatus(Profile* new_profile) = 0;

  // Perform any set up that is required after profile is initialized, such as
  // cookie migration for token based profiles.
  virtual void OnManagedProfileInitialized(
      Profile* source_profile,
      Profile* new_profile,
      ProfileCreationCallback callback) = 0;
};

// Base level class for creating managed profiles.
class ManagedProfileCreator : public ProfileAttributesStorageObserver,
                              public ProfileManagerObserver {
 public:
  // Creates a new managed profile by using the provided `delegate`.
  // The callback is called with the new profile or nullptr in case of failure.
  // The callback is never called synchronously.
  // If |local_profile_name| is not empty, it will be set as local name for the
  // new profile.
  ManagedProfileCreator(
      Profile* source_profile,
      const std::string& id,
      const std::u16string& local_profile_name,
      std::unique_ptr<ManagedProfileCreationDelegate> delegate,
      ProfileCreationCallback callback,
      std::string preset_guid = std::string());

  // Uses this version when the profile already exists at `target_profile_path`
  // but may not be loaded in memory. The profile is loaded if necessary.
  ManagedProfileCreator(
      Profile* source_profile,
      const base::FilePath& target_profile_path,
      std::unique_ptr<ManagedProfileCreationDelegate> delegate,
      ProfileCreationCallback callback);

  ~ManagedProfileCreator() override;
  ManagedProfileCreator(const ManagedProfileCreator&) = delete;
  ManagedProfileCreator& operator=(const ManagedProfileCreator&) = delete;

  // ProfileAttributesStorageObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;

  // ProfileManagerObserver:
  void OnProfileCreationStarted(Profile* profile) override;
  void OnProfileAdded(Profile* profile) override {}

 private:
  void OnNewProfileCreated(Profile* new_profile);
  void OnNewProfileInitialized(Profile* new_profile);

  raw_ptr<Profile, DanglingUntriaged> source_profile_;
  const std::string id_;
  std::unique_ptr<ManagedProfileCreationDelegate> delegate_;
  base::FilePath expected_profile_path_;
  ProfileCreationCallback callback_;
  std::string preset_guid_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  base::WeakPtrFactory<ManagedProfileCreator> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_CREATOR_H_
