// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/avatar_menu.h"

class ProfileAttributesStorage;

// This model represents the profiles added to desktop Chrome. Omitted profiles
// are not added to the menu.
class ProfileListDesktop {
 public:
  explicit ProfileListDesktop(ProfileAttributesStorage* profile_storage);

  ProfileListDesktop(const ProfileListDesktop&) = delete;
  ProfileListDesktop& operator=(const ProfileListDesktop&) = delete;

  ~ProfileListDesktop();

  // Returns the number of profiles in the model.
  size_t GetNumberOfItems() const;

  // Returns the Item at the specified index.
  const AvatarMenu::Item& GetItemAt(size_t index) const;

  // Rebuilds the menu from the data source.
  void RebuildMenu();

  // Returns the menu index of the profile with `path`, or nullopt if there is
  // no menu entry corresponding to this path (e.g. if the profile is omitted,
  // or if it's guest).
  std::optional<size_t> MenuIndexFromProfilePath(
      const base::FilePath& path) const;

  // Updates the path of the active browser's profile.
  void ActiveProfilePathChanged(const base::FilePath& active_profile_path);

 private:
  // The storage that provides the profile attributes. Not owned.
  raw_ptr<ProfileAttributesStorage, DanglingUntriaged> profile_storage_;

  // The path of the currently active profile.
  base::FilePath active_profile_path_;

  // List of built "menu items."
  std::vector<std::unique_ptr<AvatarMenu::Item>> items_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_
