// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"

// Internal free-standing functions that are exported here for testing.
namespace profiles {
namespace internal {

// Returns the full path to the profile icon file.
base::FilePath GetProfileIconPath(const base::FilePath& profile_path);

// Returns the default shortcut filename for the given profile name. Returns a
// filename appropriate for a single-user installation if |profile_name| is
// empty.
base::string16 GetShortcutFilenameForProfile(
    const base::string16& profile_name);

// The same as GetShortcutFilenameForProfile but uniqueness is guaranteed.
// Makes an unique filename among |excludes|.
base::string16 GetUniqueShortcutFilenameForProfile(
    const base::string16& profile_name,
    const std::set<base::FilePath>& excludes);

// This class checks that shortcut filename matches certain profile.
class ShortcutFilenameMatcher {
 public:
  explicit ShortcutFilenameMatcher(const base::string16& profile_name);

  // Check that shortcut filename has a name given by us (by
  // GetShortcutFilenameForProfile or GetUniqueShortcutFilenameForProfile).
  bool IsCanonical(const base::string16& filename) const;

 private:
  const base::string16 profile_shortcut_filename_;
  const base::StringPiece16 lnk_ext_;
  base::StringPiece16 profile_shortcut_name_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutFilenameMatcher);
};

// Returns the command-line flags to launch Chrome with the given profile.
base::string16 CreateProfileShortcutFlags(const base::FilePath& profile_path);

}  // namespace internal
}  // namespace profiles

class ProfileShortcutManagerWin : public ProfileShortcutManager,
                                  public ProfileAttributesStorage::Observer,
                                  public ProfileManagerObserver {
 public:
  // Specifies whether only the existing shortcut should be updated, a new
  // shortcut should be created if none exist, or only the icon for this profile
  // should be created in the profile directory.
  enum CreateOrUpdateMode {
    UPDATE_EXISTING_ONLY,
    CREATE_WHEN_NONE_FOUND,
    CREATE_OR_UPDATE_ICON_ONLY,
  };
  // Specifies whether non-profile shortcuts should be updated. This also
  // includes default profile shortcuts, which point at the default
  // profile, but don't have a profile name in their filename.
  enum NonProfileShortcutAction {
    IGNORE_NON_PROFILE_SHORTCUTS,
    UPDATE_NON_PROFILE_SHORTCUTS,
  };

  explicit ProfileShortcutManagerWin(ProfileManager* manager);
  ~ProfileShortcutManagerWin() override;

  // ProfileShortcutManager implementation:
  void CreateOrUpdateProfileIcon(const base::FilePath& profile_path) override;
  void CreateProfileShortcut(const base::FilePath& profile_path) override;
  void RemoveProfileShortcuts(const base::FilePath& profile_path) override;
  void HasProfileShortcuts(const base::FilePath& profile_path,
                           const base::Callback<void(bool)>& callback) override;
  void GetShortcutProperties(const base::FilePath& profile_path,
                             base::CommandLine* command_line,
                             base::string16* name,
                             base::FilePath* icon_path) override;

  // ProfileAttributesStorage::Observer implementation:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

 private:
  // Gives the profile path of an alternate profile than |profile_path|.
  // Must only be called when the number profiles is 2.
  base::FilePath GetOtherProfilePath(const base::FilePath& profile_path);

  // Creates or updates shortcuts for the profile at |profile_path| according
  // to the specified |create_mode| and |action|. This will always involve
  // creating or updating the icon file for this profile.
  void CreateOrUpdateShortcutsForProfileAtPath(
      const base::FilePath& profile_path,
      CreateOrUpdateMode create_mode,
      NonProfileShortcutAction action);

  ProfileManager* profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManagerWin);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
