// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/buildflags.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "ui/gfx/image/image.h"

class AvatarMenuObserver;
class Browser;
class ProfileAttributesStorage;
class ProfileListDesktop;

// This class represents the menu-like interface used to select profiles,
// such as the bubble that appears when the avatar icon is clicked in the
// browser window frame. This class will notify its observer when the backend
// data changes, and the view for this model should forward actions
// back to it in response to user events.
class AvatarMenu :
    public SupervisedUserServiceObserver,
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
    std::u16string name;

    // A string representing the username of the profile, if signed in.  Empty
    // when not signed in.
    std::u16string username;

    // Whether or not the current profile requires sign-in before use.
    bool signin_required;

    // The index in the menu of this profile, used by views to refer to
    // profiles.
    size_t menu_index;

    // The path of this profile.
    base::FilePath profile_path;
  };

  // The load status of an avatar image.
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
    // The image could not be loaded because the browser is shutting down.
    BROWSER_SHUTTING_DOWN,
  };

  // Constructor. |observer| can be NULL. |browser| can be NULL and a new one
  // will be created if an action requires it.
  AvatarMenu(ProfileAttributesStorage* profile_storage,
             AvatarMenuObserver* observer,
             Browser* browser);

  AvatarMenu(const AvatarMenu&) = delete;
  AvatarMenu& operator=(const AvatarMenu&) = delete;

  ~AvatarMenu() override;

  // Sets |image| to the avatar corresponding to the profile at |profile_path|.
  // Returns the image load status.
  static ImageLoadStatus GetImageForMenuButton(
      const base::FilePath& profile_path,
      gfx::Image* image,
      int preferred_size);

  // Opens a Browser with the specified profile in response to the user
  // selecting an item. If |always_create| is true then a new window is created
  // even if a window for that profile already exists.
  void SwitchToProfile(size_t index, bool always_create);

  // Creates a new profile.
  void AddNewProfile();

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
  size_t GetIndexOfItemWithProfilePathForTesting(
      const base::FilePath& path) const;

  // Returns the index of the active profile or `std::nullopt` if there is no
  // active profile.
  std::optional<size_t> GetActiveProfileIndex() const;

  // This menu is also used for the always-present Mac and Linux system menubar.
  // If the last active browser changes, the menu will need to reference that
  // browser.
  void ActiveBrowserChanged(Browser* browser);

  // Returns true if the add profile link should be shown/enabled.
  bool ShouldShowAddNewProfileLink() const;

  // Returns true if the edit profile link should be shown/enabled.
  bool ShouldShowEditProfileLink() const;

 private:
  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;

  // SupervisedUserServiceObserver:
  void OnCustodianInfoChanged() override;

  // Rebuilds the menu and notifies any observers that an update occured.
  void Update();

  // The model that provides the list of menu items.
  std::unique_ptr<ProfileListDesktop> profile_list_;

  // Observes changes to a supervised user's custodian info.
  base::ScopedObservation<supervised_user::SupervisedUserService,
                          SupervisedUserServiceObserver>
      supervised_user_observation_{this};

  // The storage that provides the profile attributes.
  base::WeakPtr<ProfileAttributesStorage> profile_storage_;

  // The observer of this model, which is notified of changes. Weak.
  raw_ptr<AvatarMenuObserver, DanglingUntriaged> observer_;

  // Browser in which this avatar menu resides. Weak.
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_;
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_H_
