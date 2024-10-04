// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_TESTING_HELPER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_TESTING_HELPER_H_

#include "base/memory/raw_ptr.h"
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

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestingProfile* system_profile() { return system_profile_; }
  Profile* system_profile_otr() { return system_profile_otr_; }
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestingProfile* signin_profile() { return signin_profile_; }
  Profile* signin_profile_otr() { return signin_profile_otr_; }

  TestingProfile* lockscreen_profile() { return lockscreen_profile_; }
  Profile* lockscreen_profile_otr() { return lockscreen_profile_otr_; }

  TestingProfile* lockscreenapp_profile() { return lockscreenapp_profile_; }
  Profile* lockscreenapp_profile_otr() { return lockscreenapp_profile_otr_; }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;

  raw_ptr<TestingProfile, DanglingUntriaged> regular_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> incognito_profile_ = nullptr;

  raw_ptr<TestingProfile, DanglingUntriaged> guest_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> guest_profile_otr_ = nullptr;

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  raw_ptr<TestingProfile, DanglingUntriaged> system_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> system_profile_otr_ = nullptr;
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfile, DanglingUntriaged> signin_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> signin_profile_otr_ = nullptr;

  raw_ptr<TestingProfile, DanglingUntriaged> lockscreen_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> lockscreen_profile_otr_ = nullptr;

  raw_ptr<TestingProfile, DanglingUntriaged> lockscreenapp_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> lockscreenapp_profile_otr_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_TESTING_HELPER_H_
