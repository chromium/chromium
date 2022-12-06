// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/user_manager/user.h"
#include "ui/gfx/image/image_skia.h"

class AccountId;
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

class UserImageSyncObserver;

class UserImageManagerImpl : public UserImageManager,
                             public ProfileDownloaderDelegate {
 public:
  // UserImageManager:
  UserImageManagerImpl(const AccountId& account_id,
                       user_manager::UserManager* user_manager);

  UserImageManagerImpl(const UserImageManagerImpl&) = delete;
  UserImageManagerImpl& operator=(const UserImageManagerImpl&) = delete;

  ~UserImageManagerImpl() override;

  void LoadUserImage() override;
  void UserLoggedIn(bool user_is_new, bool user_is_local) override;
  void UserProfileCreated() override;
  void SaveUserDefaultImageIndex(int default_image_index) override;
  void SaveUserImage(
      std::unique_ptr<user_manager::UserImage> user_image) override;
  void SaveUserImageFromFile(const base::FilePath& path) override;
  void SaveUserImageFromProfileImage() override;
  void DeleteUserImage() override;
  void DownloadProfileImage() override;
  const gfx::ImageSkia& DownloadedProfileImage() const override;
  UserImageSyncObserver* GetSyncObserver() const override;
  void Shutdown() override;

  bool IsUserImageManaged() const override;
  void OnExternalDataSet(const std::string& policy) override;
  void OnExternalDataCleared(const std::string& policy) override;
  void OnExternalDataFetched(const std::string& policy,
                             std::unique_ptr<std::string> data) override;

  // Sets the `downloaded_profile_image_` without downloading for testing.
  void SetDownloadedProfileImageForTesting(const gfx::ImageSkia& image);

  static void IgnoreProfileDataDownloadDelayForTesting();
  static void SkipProfileImageDownloadForTesting();
  static void SkipDefaultUserImageDownloadForTesting();

  // Key for a dictionary that maps user IDs to user image data with images
  // stored in JPEG format.
  static const char kUserImageProperties[];
  // Names of user image properties.
  static const char kImagePathNodeName[];
  static const char kImageIndexNodeName[];
  static const char kImageURLNodeName[];
  static const char kImageCacheUpdated[];

 private:
  friend class UserImageManagerTestBase;

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

  // The user manager.
  user_manager::UserManager* user_manager_;

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

  // Whether a download of the currently logged-in user's profile image has been
  // explicitly requested by a call to DownloadProfileImage() and has not been
  // satisfied by a successful download yet.
  bool profile_image_requested_;

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
