// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/user_manager/user.h"
#include "ui/gfx/image/image_skia.h"

class AccountId;
class PrefRegistrySimple;
class ProfileDownloader;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace user_manager {
class UserImage;
class UserManager;
}  // namespace user_manager

namespace ash {

class UserImageLoaderDelegate;
class UserImageSyncObserver;

// Provides a mechanism for updating user images. There is an instance of this
// class for each user in the system.
class UserImageManagerImpl : public ProfileDownloaderDelegate {
 public:
  // The name of the histogram that records when a user changes a device image.
  inline static constexpr char kUserImageChangedHistogramName[] =
      "UserImage.Changed2";

  // The name of the histogram that records the user's chosen image at login.
  inline static constexpr char kUserImageLoggedInHistogramName[] =
      "UserImage.LoggedIn3";

  // Converts `image_index` to UMA histogram value.
  static int ImageIndexToHistogramIndex(int image_index);

  // See histogram values in default_user_images.cc
  static void RecordUserImageChanged(int histogram_value);

  // Registers user image manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  UserImageManagerImpl(const AccountId& account_id,
                       user_manager::UserManager* user_manager,
                       UserImageLoaderDelegate* user_image_loader_delegate);

  UserImageManagerImpl(const UserImageManagerImpl&) = delete;
  UserImageManagerImpl& operator=(const UserImageManagerImpl&) = delete;

  ~UserImageManagerImpl() override;

  // Loads user image data from Local State.
  void LoadUserImage();

  // Indicates that a user has just logged in.
  void UserLoggedIn(bool user_is_new, bool user_is_local);

  // Indicates that a user profile was created.
  void UserProfileCreated();

  // Sets user image to the default image with index `image_index`, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  void SaveUserDefaultImageIndex(int default_image_index);

  // Saves image to file, sends LOGIN_USER_IMAGE_CHANGED notification and
  // updates Local State.
  void SaveUserImage(std::unique_ptr<user_manager::UserImage> user_image);

  // Tries to load user image from disk; if successful, sets it for the user,
  // sends LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  void SaveUserImageFromFile(const base::FilePath& path);

  // Sets profile image as user image for the user, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State. If
  // the user is not logged-in or the last `DownloadProfileImage` call
  // has failed, a default grey avatar will be used until the user logs
  // in and profile image is downloaded successfully.
  void SaveUserImageFromProfileImage();

  // Deletes user image and the corresponding image file.
  void DeleteUserImage();

  // Returns the result of the last successful profile image download, if any.
  // Otherwise, returns an empty bitmap.
  const gfx::ImageSkia& DownloadedProfileImage() const;

  // Returns sync observer attached to the user. Returns NULL if current
  // user can't sync images or user is not logged in.
  UserImageSyncObserver* GetSyncObserver() const;

  // Unregisters preference observers before browser process shutdown.
  // Also cancels any profile image download in progress.
  void Shutdown();

  // Returns true if the user image for the user is managed by
  // policy and the user is not allowed to change it.
  bool IsUserImageManaged() const;

  // Invoked when an external data reference is set for the user.
  void OnExternalDataSet(const std::string& policy);

  // Invoked when the external data reference is cleared for the user.
  void OnExternalDataCleared(const std::string& policy);

  // Invoked when the external data referenced for the user has been
  // fetched.  Failed fetches are retried and the method is called only
  // when a fetch eventually succeeds. If a fetch fails permanently
  // (e.g. because the external data reference specifies an invalid URL),
  // the method is not called at all.
  void OnExternalDataFetched(const std::string& policy,
                             std::unique_ptr<std::string> data);

  // Sets the `downloaded_profile_image_` without downloading for testing.
  void SetDownloadedProfileImageForTesting(const gfx::ImageSkia& image);

  static void IgnoreProfileDataDownloadDelayForTesting();
  static void SkipProfileImageDownloadForTesting();
  static void SkipDefaultUserImageDownloadForTesting();

  // Key for a dictionary that maps user IDs to user image data with images
  // stored in JPEG format.
  static constexpr char kUserImageProperties[] = "user_image_info";

  // Names of user image properties.
  static constexpr char kImagePathNodeName[] = "path";
  static constexpr char kImageIndexNodeName[] = "index";
  static constexpr char kImageURLNodeName[] = "url";
  static constexpr char kImageCacheUpdated[] = "cache_updated";

 private:
  friend class UserImageManagerTestBase;
  friend class UserImageManagerImplTest;

  // ID of user which images are managed by current instance of
  // UserImageManager.
  const AccountId account_id_;

  // Every image load or update is encapsulated by a Job. Whenever an image load
  // or update is requested for a user, the Job currently running for that user
  // (if any) is canceled. This ensures that at most one Job is running per user
  // at any given time. There are two further guarantees:
  //
  // * Changes to User objects and local state are performed on the thread that
  //   `this` runs on.
  // * File writes and deletions are performed via `background_task_runner_`.
  //
  // With the above, it is guaranteed that any changes made by a canceled Job
  // cannot race against against changes made by the superseding Job.
  class Job;

  // ProfileDownloaderDelegate:
  bool NeedsProfilePicture() const override;
  int GetDesiredImageSideLength() const override;
  signin::IdentityManager* GetIdentityManager() override;
  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override;
  std::string GetCachedPictureURL() const override;
  void OnProfileDownloadSuccess(ProfileDownloader* downloader) override;
  void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) override;

  // Randomly chooses one of the default images for the specified user, sends a
  // LOGIN_USER_IMAGE_CHANGED notification and updates local state.
  void SetInitialUserImage();

  // Initializes the `downloaded_profile_image_` for the currently logged-in
  // user to a profile image that had been downloaded and saved before if such
  // a saved image is available and no updated image has been downloaded yet.
  void TryToInitDownloadedProfileImage();

  // Returns true if the profile image needs to be downloaded. This is the case
  // when a GAIA user is logged in and at least one of the following applies:
  // * The profile image has explicitly been requested by a call to
  //   DownloadProfileImage() and has not been successfully downloaded since.
  // * The user's user image is the profile image.
  bool NeedProfileImage() const;

  // Downloads the profile data for the currently logged-in user. The user's
  // full name and, if NeedProfileImage() is true, the profile image are
  // downloaded.
  void DownloadProfileData();

  // Removes ther user from the dictionary `prefs_dict_root` in
  // local state and deletes the image file that the dictionary
  // referenced for that user.
  void DeleteUserImageAndLocalStateEntry(const char* prefs_dict_root);

  // Called when a Job updates the copy of the user image held in
  // memory.  Allows `this` to update `downloaded_profile_image_` and
  // notify user manager about user image change.
  void OnJobChangedUserImage();

  // Called when a Job for the user finishes.
  void OnJobDone();

  // Create a sync observer if a user is logged in, the user's user image is
  // allowed to be synced and no sync observer exists yet.
  void TryToCreateImageSyncObserver();

  // Returns the image properties for the user's user image.
  const base::Value::Dict* GetImageProperties();

  // Returns immutable version of user with `user_id_`.
  const user_manager::User* GetUser() const;

  // Returns mutable version of user with `user_id_`.
  user_manager::User* GetUserAndModify() const;

  // Returns true if user with `user_id_` is logged in and has gaia account.
  bool IsUserLoggedInAndHasGaiaAccount() const;

  // Returns true if user avatar customization selectors are enabled. Profile
  // image download will only occur if this returns true
  bool IsCustomizationSelectorsPrefEnabled() const;

  // The user manager.
  raw_ptr<user_manager::UserManager> user_manager_;

  // A delegate to retrieve user images from disk and network. Allows injecting
  // a mock for testing.
  raw_ptr<UserImageLoaderDelegate> user_image_loader_delegate_;

  // Whether the `profile_downloader_` is downloading the profile image for the
  // currently logged-in user (and not just the full name). Only valid when a
  // download is currently in progress.
  bool downloading_profile_image_;

  // Downloader for the user's profile data. NULL when no download is
  // currently in progress.
  std::unique_ptr<ProfileDownloader> profile_downloader_;

  // The currently logged-in user's downloaded profile image, if successfully
  // downloaded or initialized from a previously downloaded and saved image.
  gfx::ImageSkia downloaded_profile_image_;

  // Data URL corresponding to `downloaded_profile_image_`. Empty if no
  // `downloaded_profile_image_` is currently available.
  std::string downloaded_profile_image_data_url_;

  // URL from which `downloaded_profile_image_` was downloaded. Empty if no
  // `downloaded_profile_image_` is currently available.
  GURL profile_image_url_;

  // Timer used to start a profile data download shortly after login and to
  // restart the download after network errors.
  base::OneShotTimer profile_download_one_shot_timer_;

  // Timer used to periodically start a profile data, ensuring the profile data
  // stays up to date.
  base::RepeatingTimer profile_download_periodic_timer_;

  // Sync observer for the currently logged-in user.
  std::unique_ptr<UserImageSyncObserver> user_image_sync_observer_;

  // Background task runner on which Jobs perform file I/O and the image
  // decoders run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The currently running job.
  std::unique_ptr<Job> job_;

  bool has_managed_image_;

  // If true user image manager trying to download and set profile image instead
  // of the random one.
  bool is_random_image_set_ = false;

  base::WeakPtrFactory<UserImageManagerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_
