// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/initial_prefs.h"

#include "base/apple/foundation_util.h"
#include "base/files/file_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/version_info/version_info.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// This should be NSApplicationSupportDirectory, but it has already been
// released using NSLibraryDirectory.
const NSSearchPathDirectory kSearchPath = NSLibraryDirectory;
// Note: the actual filename here still contains the word "master" despite the
// migration of the rest of this code to more inclusive language. Unfortunately
// the file with this filename is the documented way to set initial preferences,
// so changing this filename will require some care.
// See https://crbug.com/1097204 for details.
const char kInitialPreferencesDirectory[] = "Google";
const char kInitialPreferencesFileName[] = "Google Chrome Initial Preferences";
const char kLegacyInitialPreferencesFileName[] =
    "Google Chrome Master Preferences";
#else
const NSSearchPathDirectory kSearchPath = NSApplicationSupportDirectory;
const char kInitialPreferencesDirectory[] = "Chromium";
const char kInitialPreferencesFileName[] = "Chromium Initial Preferences";
const char kLegacyInitialPreferencesFileName[] = "Chromium Master Preferences";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

namespace initial_prefs {

base::FilePath InitialPrefsPath() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Don't load initial preferences for the canary.
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::CANARY)
    return base::FilePath();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // On official builds, try
  //~/Library/Application Support/Google/Chrome/Google Chrome Master Preferences
  // On chromium builds, try
  //~/Library/Application Support/Chromium/Chromium Master Preferences
  // This intentionally doesn't use eventual --user-data-dir overrides.
  base::FilePath user_application_support_path;
  if (chrome::GetDefaultUserDataDirectory(&user_application_support_path)) {
    base::FilePath new_path =
        user_application_support_path.Append(kInitialPreferencesFileName);
    if (base::PathExists(new_path))
      return new_path;

    base::FilePath old_path =
        user_application_support_path.Append(kLegacyInitialPreferencesFileName);
    if (base::PathExists(old_path))
      return old_path;
  }

  // On official builds, try /Library/Google/Google Chrome Master Preferences
  // On chromium builds, try
  // /Library/Application Support/Chromium/Chromium Master Preferences
  base::FilePath search_path;
  if (!base::apple::GetLocalDirectory(kSearchPath, &search_path)) {
    return base::FilePath();
  }

  base::FilePath new_path = search_path.Append(kInitialPreferencesDirectory)
                                .Append(kInitialPreferencesFileName);
  if (base::PathExists(new_path))
    return new_path;

  return search_path.Append(kInitialPreferencesDirectory)
      .Append(kLegacyInitialPreferencesFileName);
}

}  // namespace initial_prefs
