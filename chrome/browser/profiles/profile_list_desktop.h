// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile_list.h"

class ProfileAttributesStorage;

// This model represents the profiles added to desktop Chrome (as opposed to
// Chrome OS). Profiles marked not to appear in the list will be omitted
// throughout.
class ProfileListDesktop : public ProfileList {
 public:
  explicit ProfileListDesktop(ProfileAttributesStorage* profile_storage);
  ~ProfileListDesktop() override;

 private:
  // ProfileList overrides:
  size_t GetNumberOfItems() const override;
  const AvatarMenu::Item& GetItemAt(size_t index) const override;
  void RebuildMenu() override;
  // Returns the menu index of the profile with |path| in the
  // ProfileAttributesStorage. The profile with the |path| must exist, and it
  // may not be marked as omitted from the menu.
  size_t MenuIndexFromProfilePath(const base::FilePath& path) const override;
  void ActiveProfilePathChanged(
      const base::FilePath& active_profile_path) override;

  // The storage that provides the profile attributes. Not owned.
  ProfileAttributesStorage* profile_storage_;

  // The path of the currently active profile.
  base::FilePath active_profile_path_;

  // List of built "menu items."
  std::vector<std::unique_ptr<AvatarMenu::Item>> items_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListDesktop);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_
