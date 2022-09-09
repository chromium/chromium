// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LIST_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LIST_H_

#include <stddef.h>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/avatar_menu.h"

class ProfileAttributesStorage;

// This model represents the profiles added to Chrome.
// Only implemented by ProfileListDesktop, although a Chrome OS version used to
// exist as ProfileListChromeOS.
class ProfileList {
 public:
  virtual ~ProfileList() {}

  static ProfileList* Create(ProfileAttributesStorage* profile_storage);

  // Returns the number of profiles in the model.
  virtual size_t GetNumberOfItems() const = 0;

  // Returns the Item at the specified index.
  virtual const AvatarMenu::Item& GetItemAt(size_t index) const = 0;

  // Rebuilds the menu from the data source.
  virtual void RebuildMenu() = 0;

  // Returns the index in the menu of the specified profile.
  virtual size_t MenuIndexFromProfilePath(const base::FilePath& path) const = 0;

  // Updates the path of the active browser's profile.
  virtual void ActiveProfilePathChanged(
      const base::FilePath& active_profile_path) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LIST_H_
