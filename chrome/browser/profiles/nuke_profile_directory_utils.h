// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_NUKE_PROFILE_DIRECTORY_UTILS_H_
#define CHROME_BROWSER_PROFILES_NUKE_PROFILE_DIRECTORY_UTILS_H_

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

// Physically remove deleted profile directories from disk.
void NukeDeletedProfilesFromDisk();

// Physically remove deleted profile directories from disk. Afterwards, calls
// |done_callback| on the UI thread.
void NukeProfileFromDisk(const base::FilePath& profile_path,
                         base::OnceClosure done_callback);

// Returns if profile is marked for deletion.
bool IsProfileDirectoryMarkedForDeletion(const base::FilePath& profile_path);

// Cancel a scheduling deletion, so ScheduleProfileDirectoryForDeletion can be
// called again successfully.
void CancelProfileDeletion(const base::FilePath& path);

// Schedule a profile for deletion if it isn't already scheduled.
// Returns whether the profile has been newly scheduled.
bool ScheduleProfileDirectoryForDeletion(const base::FilePath& path);

// Marks the profile path for deletion. It will be deleted when
// `NukeDeletedProfilesFromDisk()` is called.
void MarkProfileDirectoryForDeletion(const base::FilePath& path);

#endif  // CHROME_BROWSER_PROFILES_NUKE_PROFILE_DIRECTORY_UTILS_H_
