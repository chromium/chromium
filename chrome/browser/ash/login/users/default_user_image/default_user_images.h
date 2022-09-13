// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/default_user_image.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace gfx {
class ImageSkia;
}

namespace ash::default_user_image {

// Enumeration of user image eligibility states.
enum class Eligibility {
  // The images has been deprecated.
  kDeprecated,
  // The image is eligible.
  kEligible,
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
GURL GetDefaultImageUrl(int index);

// Returns bitmap of default user image with specified index.
const gfx::ImageSkia& GetDefaultImage(int index);

// Returns ID of default user image with specified index.
int GetDefaultImageResourceId(int index);

// Returns a random default image index.
int GetRandomDefaultImageIndex();

// Returns true if `index` is a valid default image index.
bool IsValidIndex(int index);

// Returns true if `index` is a in the current set of default images.
bool IsInCurrentImageSet(int index);

DefaultUserImage GetDefaultUserImage(int index);

// Returns a vector of current |DefaultUserImage|.
std::vector<DefaultUserImage> GetCurrentImageSet();

base::Value::List GetCurrentImageSetAsListValue();

// Returns the source info of the default user image with specified index.
// Returns nullopt if there is no source info.
// Only a small number of deprecated user images have associated
// |DeprecatedSourceInfo|, and none of them can be selected by users now.
absl::optional<DeprecatedSourceInfo> GetDeprecatedDefaultImageSourceInfo(
    size_t index);

}  // namespace ash::default_user_image

// TODO(https://crbug.com/1164001): remove once the migration is finished.
namespace chromeos {
namespace default_user_image = ::ash::default_user_image;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
