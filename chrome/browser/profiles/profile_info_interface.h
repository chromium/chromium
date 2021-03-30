// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_INTERFACE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_INTERFACE_H_

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"

namespace gfx {
class Image;
}

// This abstract interface is used to query the profiles backend for information
// about the different profiles. Its sole concrete implementation is the
// ProfileInfoCache. This interface exists largely to assist in testing.
// The ProfileInfoInterface is being deprecated. Prefer using the
// ProfileAttributesStorage and avoid using the Get*AtIndex family of functions.
class ProfileInfoInterface {
 public:
  virtual size_t GetNumberOfProfiles(
      bool include_guest_profile = false) const = 0;

  virtual size_t GetIndexOfProfileWithPath(
      const base::FilePath& profile_path) const = 0;

  virtual base::FilePath GetPathOfProfileAtIndex(size_t index) const = 0;

  virtual const gfx::Image* GetGAIAPictureOfProfileAtIndex(
      size_t index) const = 0;

  // Checks if the GAIA picture should be used as the profile's avatar icon.
  virtual bool IsUsingGAIAPictureOfProfileAtIndex(size_t index) const = 0;

  // Returns true if the user has never manually selected a profile avatar.
  virtual bool ProfileIsUsingDefaultAvatarAtIndex(size_t index) const = 0;

 protected:
  virtual ~ProfileInfoInterface() {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_INTERFACE_H_
