// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#endif

class AvatarMenuActions;
class AvatarMenuObserver;
class Browser;
class ProfileAttributesStorage;
class ProfileList;

// This class represents the menu-like interface used to select profiles,
// such as the bubble that appears when the avatar icon is clicked in the
// browser window frame. This class will notify its observer when the backend
// data changes, and the view for this model should forward actions
// back to it in response to user events.
class AvatarMenu :
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    public SupervisedUserServiceObserver,
#endif
    public ProfileAttributesStorage::Observer {
 public:
  // Represents an item in the menu.
  struct Item {
    Item(size_t menu_index, const base::FilePath& profile_path,
         const gfx::Image& icon);
    Item(const Item& other);
    ~Item();

    // The icon to be displayed next to the item.
    gfx::Image icon;

    // Whether or not the current browser is using this profile.
    bool active;

    // The name of this profile.
    base::string16 name;

    // A string representing the username of the profile, if signed in.  Empty
    // when not signed in.
    base::string16 username;

    // Whether or not the current profile is signed in. If true, |sync_state| is
    // expected to be the email of the signed in user.
    bool signed_in;

    // Whether or not the current profile requires sign-in before use.
    bool signin_required;

    // Whether or not the current profile is a legacy supervised user profile
    // (see SupervisedUserService).
    bool legacy_supervised;

    // Whether or not the profile is associated with a child account
    // (see SupervisedUserService).
    bool child_account;

    // The index in the menu of this profile, used by views to refer to
    // profiles.
    size_t menu_index;

    // The path of this profile.
    base::FilePath profile_path;
  };

  // The load status of an avatar image. This is used to back an UMA histogram,
  // and should therefore be treated as append-only.
  enum class ImageLoadStatus {
    // If there is a Gaia image used by the user, it is loaded. Otherwise, a
    // default avatar image is loaded.
    LOADED = 0,
    // There is a Gaia image used by the user, and it's still being loaded.
    LOADING,
    // There is a Gaia image used by the user, but it cannot be found. A
    // default avatar image is loaded instead.
    MISSING,
    // Nothing is loaded as the profile has been deleted.
    PROFILE_DELETED,
    // This is always the last one.
    MAX = PROFILE_DELETED
  };

  // Constructor. |observer| can be NULL. |browser| can be NULL and a new one
  // will be created if an action requires it.
  AvatarMenu(ProfileAttributesStorage* profile_storage,
             AvatarMenuObserver* observer,
             Browser* browser);
  ~AvatarMenu() override;

  // Sets |image| to the avatar corresponding to the profile at |profile_path|.
  // For built-in profile avatars, returns the non-high res version. Returns the
  // image load status.
  static ImageLoadStatus GetImageForMenuButton(
      const base::FilePath& profile_path,
      gfx::Image* image);

  // Opens a Browser with the specified profile in response to the user
  // selecting an item. If |always_create| is true then a new window is created
  // even if a window for that profile already exists.
  void SwitchToProfile(size_t index,
                       bool always_create,
                       ProfileMetrics::ProfileOpen metric);

  // Creates a new profile.
  void AddNewProfile(ProfileMetrics::ProfileAdd type);

  // Opens the profile settings in response to clicking the edit button next to
  // an item.
  void EditProfile(size_t index);

  // Rebuilds the menu from the cache. Note: If this is done in response to the
  // active browser changing, ActiveBrowserChanged() should be called first to
  // update this object's internal state.
  void RebuildMenu();

  // Gets the number of profiles.
  size_t GetNumberOfItems() const;

  // Gets the Item at the specified index.
  const Item& GetItemAt(size_t index) const;

  // Gets the index in this menu for which profile_path is equal to |path|.
  size_t GetIndexOfItemWithProfilePath(const base::FilePath& path);

  // Returns the index of the active profile.
  size_t GetActiveProfileIndex();

  // Returns information about a supervised user which will be displayed in the
  // avatar menu. If the profile does not belong to a supervised user, an empty
  // string will be returned.
  base::string16 GetSupervisedUserInformation() const;

  // This menu is also used for the always-present Mac and Linux system menubar.
  // If the last active browser changes, the menu will need to reference that
  // browser.
  void ActiveBrowserChanged(Browser* browser);

  // Returns true if the add profile link should be shown.
  bool ShouldShowAddNewProfileLink() const;

  // Returns true if the edit profile link should be shown.
  bool ShouldShowEditProfileLink() const;

 private:
  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
      const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
      const base::string16& old_profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // SupervisedUserServiceObserver:
  void OnCustodianInfoChanged() override;
#endif

  // Rebuilds the menu and notifies any observers that an update occured.
  void Update();

  // The model that provides the list of menu items.
  std::unique_ptr<ProfileList> profile_list_;

  // The controller for avatar menu actions.
  std::unique_ptr<AvatarMenuActions> menu_actions_;

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Observes changes to a supervised user's custodian info.
  ScopedObserver<SupervisedUserService, SupervisedUserServiceObserver>
      supervised_user_observer_{this};
#endif

  // The storage that provides the profile attributes.
  base::WeakPtr<ProfileAttributesStorage> profile_storage_;

  // The observer of this model, which is notified of changes. Weak.
  AvatarMenuObserver* observer_;

  // Browser in which this avatar menu resides. Weak.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenu);
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_H_
