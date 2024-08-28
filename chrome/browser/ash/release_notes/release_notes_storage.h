// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_RELEASE_NOTES_RELEASE_NOTES_STORAGE_H_
#define CHROME_BROWSER_ASH_RELEASE_NOTES_RELEASE_NOTES_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class Profile;
class PrefRegistrySimple;

namespace ash {

// This stores the latest milestone with new Release Notes content. If the last
// milestone the user has seen the notification is before this, a new
// notification will be shown.
inline constexpr int kLastChromeVersionWithReleaseNotes = 130;

// Class used to determine when/if to show user notification that release notes
// are available for their recently updated device.
class ReleaseNotesStorage {
 public:
  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit ReleaseNotesStorage(Profile* profile);

  ReleaseNotesStorage(const ReleaseNotesStorage&) = delete;
  ReleaseNotesStorage& operator=(const ReleaseNotesStorage&) = delete;

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

  // Sets the number of times left to show the chip to the max number.
  void StartShowingSuggestionChip();

  // Sets the number of times left to show the suggestion chip to 0.
  void StopShowingSuggestionChip();

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_RELEASE_NOTES_RELEASE_NOTES_STORAGE_H_
