// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_UNITTEST_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_UNITTEST_H_

#include <set>

#include "base/macros.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileInfoCache;

namespace base {
class FilePath;
}

// Class used to test that ProfileInfoCache does not try to access any
// unexpected profile names.
class ProfileNameVerifierObserver : public ProfileInfoCacheObserver {
 public:
  explicit ProfileNameVerifierObserver(
      TestingProfileManager* testing_profile_manager);
  ~ProfileNameVerifierObserver() override;

  // ProfileInfoCacheObserver overrides:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  ProfileInfoCache* GetCache();
  std::map<base::FilePath, base::string16> profile_names_;
  TestingProfileManager* testing_profile_manager_;
  DISALLOW_COPY_AND_ASSIGN(ProfileNameVerifierObserver);
};

class ProfileInfoCacheTest : public testing::Test {
 protected:
  ProfileInfoCacheTest();
  ~ProfileInfoCacheTest() override;

  void SetUp() override;
  void TearDown() override;

  ProfileInfoCache* GetCache();
  base::FilePath GetProfilePath(const std::string& base_name);
  void ResetCache();
  void RemoveObserver();

 private:
  // BrowserTaskEnvironment needs to be up through the destruction of the
  // TestingProfileManager below.
  content::BrowserTaskEnvironment task_environment_;

 protected:
  TestingProfileManager testing_profile_manager_;

 private:
  ProfileNameVerifierObserver name_observer_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_UNITTEST_H_
