// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_

#include <stddef.h>

#include <memory>
#include <string>


namespace base {
class ListValue;
}

namespace gfx {
class ImageSkia;
}

namespace ash {
namespace default_user_image {

// Returns the URL to a default user image with the specified index. If the
// index is invalid, returns the default user image for index 0 (anonymous
// avatar image).
std::string GetDefaultImageUrl(int index);

// Checks if the given URL points to one of the default images. If it is,
// returns true and its index through `image_id`. If not, returns false.
bool IsDefaultImageUrl(const std::string& url, int* image_id);

// Returns bitmap of default user image with specified index.
const gfx::ImageSkia& GetDefaultImage(int index);

// Resource IDs of default user images.
extern const int kDefaultImageResourceIDs[];

// Number of default images.
extern const int kDefaultImagesCount;

// The starting index of default images available for selection. Note that
// existing users may have images with smaller indices.
extern const int kFirstDefaultImageIndex;

/// Histogram values. ////////////////////////////////////////////////////////

// Histogram value for user image taken from file.
extern const int kHistogramImageFromFile;

// Histogram value for user image taken from camera.
extern const int kHistogramImageFromCamera;

// Histogram value a previously used image from camera/file.
extern const int kHistogramImageOld;

// Histogram value for user image from G+ profile.
extern const int kHistogramImageFromProfile;

// Number of possible histogram values for user images.
extern const int kHistogramImagesCount;

// Returns the histogram value corresponding to the given default image index.
int GetDefaultImageHistogramValue(int index);

// Returns a random default image index.
int GetRandomDefaultImageIndex();

// Returns true if `index` is a valid default image index.
bool IsValidIndex(int index);

// Returns true if `index` is a in the current set of default images.
bool IsInCurrentImageSet(int index);

// Returns a list of dictionary values with url, author, website, and title
// properties set for each default user image. If `all` is true then returns
// the complete list of default images, otherwise only returns the current list.
std::unique_ptr<base::ListValue> GetAsDictionary(bool all);

// Returns the index of the first default image to make available for selection
// from GetAsDictionary when `all` is true. The last image to make available is
// always the last image in the Dictionary.
int GetFirstDefaultImage();

}  // namespace default_user_image
}  // namespace ash

// TODO(https://crbug.com/1164001): remove once the migration is finished.
namespace chromeos {
namespace default_user_image = ::ash::default_user_image;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
