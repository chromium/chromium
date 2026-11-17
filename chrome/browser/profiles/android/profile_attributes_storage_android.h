// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_ANDROID_PROFILE_ATTRIBUTES_STORAGE_ANDROID_H_
#define CHROME_BROWSER_PROFILES_ANDROID_PROFILE_ATTRIBUTES_STORAGE_ANDROID_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"

class ProfileAttributesStorage;

namespace base {
class FilePath;
}

// Native counterpart of the Java `ProfileAttributesStorage`. Forwards the
// events of the observed storage to Java.
class ProfileAttributesStorageAndroid
    : public ProfileAttributesStorageObserver {
 public:
  // `storage` must outlive this object.
  explicit ProfileAttributesStorageAndroid(ProfileAttributesStorage& storage);
  ProfileAttributesStorageAndroid(const ProfileAttributesStorageAndroid&) =
      delete;
  ProfileAttributesStorageAndroid& operator=(
      const ProfileAttributesStorageAndroid&) = delete;
  ~ProfileAttributesStorageAndroid() override;

  // ProfileAttributesStorageObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      storage_observation_{this};
};

#endif  // CHROME_BROWSER_PROFILES_ANDROID_PROFILE_ATTRIBUTES_STORAGE_ANDROID_H_
