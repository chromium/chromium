// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class ListValue;
}

namespace gfx {
class ImageSkia;
}

namespace ash {
namespace default_user_image {

// Enumeration of user image eligibility states.
enum class Eligibility {
  // The images has been deprecated.
  kDeprecated,
  // The image is eligible.
  kEligible,
};

// Source info of the default user image.
struct DefaultImageSourceInfo {
  // Message IDs of author info.
  const int author_id;

  // Message IDs of website info.
  const int website_id;
};

// Number of default images.
extern const int kDefaultImagesCount;

// The starting index of default images available for selection. Note that
// existing users may have images with smaller indices.
extern const int kFirstDefaultImageIndex;

// Histogram value for user image selected from file or photo.
extern const int kHistogramImageExternal;
// Histogram value for a user image taken from the camera.
extern const int kHistogramImageFromCamera;
// Histogram value for user image from G+ profile.
extern const int kHistogramImageFromProfile;
// Max number of special histogram values for user images.
extern const int kHistogramSpecialImagesMaxCount;
// Number of possible histogram values for user images.
extern const int kHistogramImagesCount;

// Returns the URL to a default user image with the specified index. If the
// index is invalid, returns the default user image for index 0 (anonymous
// avatar image).
std::string GetDefaultImageUrl(int index);

// Checks if the given URL points to one of the default images. If it is,
// returns true and its index through `image_id`. If not, returns false.
bool IsDefaultImageUrl(const std::string& url, int* image_id);

// Returns bitmap of default user image with specified index.
const gfx::ImageSkia& GetDefaultImage(int index);

// Returns ID of default user image with specified index.
const int GetDefaultImageResourceId(int index);

// Returns a random default image index.
int GetRandomDefaultImageIndex();

// Returns true if `index` is a valid default image index.
bool IsValidIndex(int index);

// Returns true if `index` is a in the current set of default images.
bool IsInCurrentImageSet(int index);

// Returns a list of dictionary values with url and title properties set for
// each default user image in the current set.
std::unique_ptr<base::ListValue> GetCurrentImageSet();

// Returns the source info of the default user image with specified index.
// Returns nullopt if there is no source info.
absl::optional<DefaultImageSourceInfo> GetDefaultImageSourceInfo(int index);

}  // namespace default_user_image
}  // namespace ash

// TODO(https://crbug.com/1164001): remove once the migration is finished.
namespace chromeos {
namespace default_user_image = ::ash::default_user_image;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
