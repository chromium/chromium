// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/master_prefs.h"

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/version_info/version_info.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// This should be NSApplicationSupportDirectory, but it has already been
// released using NSLibraryDirectory.
const NSSearchPathDirectory kSearchPath = NSLibraryDirectory;
const char kMasterPreferencesDirectory[] = "Google";
const char kMasterPreferencesFileName[] = "Google Chrome Master Preferences";
#else
const NSSearchPathDirectory kSearchPath = NSApplicationSupportDirectory;
const char kMasterPreferencesDirectory[] = "Chromium";
const char kMasterPreferencesFileName[] = "Chromium Master Preferences";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace


namespace master_prefs {

base::FilePath MasterPrefsPath() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Don't load master preferences for the canary.
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
    user_application_support_path =
        user_application_support_path.Append(kMasterPreferencesFileName);
    if (base::PathExists(user_application_support_path))
      return user_application_support_path;
  }

  // On official builds, try /Library/Google/Google Chrome Master Preferences
  // On chromium builds, try
  // /Library/Application Support/Chromium/Chromium Master Preferences
  base::FilePath search_path;
  if (!base::mac::GetLocalDirectory(kSearchPath, &search_path))
    return base::FilePath();

  return search_path.Append(kMasterPreferencesDirectory)
                    .Append(kMasterPreferencesFileName);
}

}  // namespace master_prefs
