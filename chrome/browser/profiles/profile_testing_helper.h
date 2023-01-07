// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_TESTING_HELPER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_TESTING_HELPER_H_

#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"

class TestingProfile;
class Profile;

// Testing helper class to provide easy access to Profile Types (both Original
// and Off The Record), Regular, Guest and System.
class ProfileTestingHelper {
 public:
  ProfileTestingHelper();
  ~ProfileTestingHelper();

  void SetUp();

  TestingProfile* regular_profile() { return regular_profile_; }
  Profile* incognito_profile() { return incognito_profile_; }

  TestingProfile* guest_profile() { return guest_profile_; }
  Profile* guest_profile_otr() { return guest_profile_otr_; }

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestingProfile* system_profile() { return system_profile_; }
  Profile* system_profile_otr() { return system_profile_otr_; }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;

  TestingProfile* regular_profile_ = nullptr;
  Profile* incognito_profile_ = nullptr;

  TestingProfile* guest_profile_ = nullptr;
  Profile* guest_profile_otr_ = nullptr;

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestingProfile* system_profile_ = nullptr;
  Profile* system_profile_otr_ = nullptr;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
};

#endif  // !CHROME_BROWSER_PROFILES_PROFILE_TESTING_HELPER_H_
