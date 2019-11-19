// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_

#include <string>

#include "base/strings/string16.h"

class ProfileDownloader;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

// Reports on success or failure of Profile download. It is OK to delete the
// |ProfileImageDownloader| instance in any of these handlers.
class ProfileDownloaderDelegate {
 public:
  // Error codes passed to OnProfileDownloadFailure.
  enum FailureReason {
    TOKEN_ERROR,          // Cannot fetch OAuth2 token.
    NETWORK_ERROR,        // Network failure while downloading profile.
    SERVICE_ERROR,        // Service returned an error or malformed reply.
    IMAGE_DECODE_FAILED,  // Cannot decode fetched image.
    INVALID_PROFILE_PICTURE_URL  // The profile picture URL is invalid.
  };

  virtual ~ProfileDownloaderDelegate() {}

  // Whether the delegate need profile picture to be downloaded.
  virtual bool NeedsProfilePicture() const = 0;

  // Returns the desired side length of the profile image. If 0, returns image
  // of the originally uploaded size.
  virtual int GetDesiredImageSideLength() const = 0;

  // Returns the cached URL. If the cache URL matches the new image URL
  // the image will not be downloaded. Return an empty string when there is no
  // cached URL.
  virtual std::string GetCachedPictureURL() const = 0;

  // Returns the IdentityManager associated with this download request.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the URLLoaderFactory to use for this download request.
  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory() = 0;

  // Returns true if the profile download is taking place before the user has
  // signed in. This can happen for example on Android and will trigger some
  // additional fetches since some information is not yet available.
  virtual bool IsPreSignin() const = 0;

  // Called when the profile download has completed successfully. Delegate can
  // query the downloader for the picture and full name.
  virtual void OnProfileDownloadSuccess(ProfileDownloader* downloader) = 0;

  // Called when the profile download has failed.
  virtual void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_
