// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_OBSERVER_H_

#include <string>

namespace base {
class FilePath;
}

// This class provides an Observer interface to watch for changes to the
// ProfileAttributesStorage.
class ProfileAttributesStorageObserver {
 public:
  ProfileAttributesStorageObserver(const ProfileAttributesStorageObserver&) =
      delete;
  ProfileAttributesStorageObserver& operator=(
      const ProfileAttributesStorageObserver&) = delete;
  virtual ~ProfileAttributesStorageObserver() = default;

  // Notifies observers that a new profile at `profile_path` was added to cache.
  // It is guaranteed to be the first observer method that can observe a new
  // profile being added to cache.
  virtual void OnProfileAdded(const base::FilePath& profile_path) {}
  // Notifies observers that a profile at `profile_path` is going to be removed
  // from cache soon.
  virtual void OnProfileWillBeRemoved(const base::FilePath& profile_path) {}
  // Notifies observers that a profile at `profile_path` was removed from cache.
  // It is guaranteed to be the first observer method that can observe the
  // profile being removed from cache.
  virtual void OnProfileWasRemoved(const base::FilePath& profile_path,
                                   const std::u16string& profile_name) {}
  virtual void OnProfileNameChanged(const base::FilePath& profile_path,
                                    const std::u16string& old_profile_name) {}
  virtual void OnProfileAuthInfoChanged(const base::FilePath& profile_path) {}
  virtual void OnProfileAvatarChanged(const base::FilePath& profile_path) {}
  virtual void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) {}
  virtual void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) {}
  virtual void OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) {}
  virtual void OnProfileIsOmittedChanged(const base::FilePath& profile_path) {}
  virtual void OnProfileThemeColorsChanged(const base::FilePath& profile_path) {
  }
  virtual void OnProfileHostedDomainChanged(
      const base::FilePath& profile_path) {}
  virtual void OnProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) {}
  virtual void OnProfileManagementEnrollmentTokenChanged(
      const base::FilePath& profile_path) {}
  virtual void OnProfileManagementIdChanged(
      const base::FilePath& profile_path) {}

 protected:
  ProfileAttributesStorageObserver() = default;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_OBSERVER_H_
