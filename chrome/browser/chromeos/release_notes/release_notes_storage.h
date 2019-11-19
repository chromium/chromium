// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_RELEASE_NOTES_RELEASE_NOTES_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_RELEASE_NOTES_RELEASE_NOTES_STORAGE_H_

#include "base/macros.h"

class Profile;
class PrefRegistrySimple;

namespace chromeos {

// Class used to determine when/if to show user notification that release notes
// are available for their recently updated device.
class ReleaseNotesStorage {
 public:
  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit ReleaseNotesStorage(Profile* profile);
  ~ReleaseNotesStorage();

  // Returns true if system has been updated since last notification, user
  // has internet connection, and user has not opted out of release notes.
  bool ShouldNotify();

  // Marks the current release as having shown the notification.
  void MarkNotificationShown();

  // Returns true if the Release Notes suggestion chip should be shown.
  bool ShouldShowSuggestionChip();

  // Decreases the amount of times left to show the suggestion chip.
  void DecreaseTimesLeftToShowSuggestionChip();

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ReleaseNotesStorage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_RELEASE_NOTES_RELEASE_NOTES_STORAGE_H_
