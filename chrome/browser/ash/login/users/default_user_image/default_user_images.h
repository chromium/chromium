// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/default_user_image.h"
#include "base/values.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

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

// The last index of legacy avatar images. Images in the legacy asset only have
// 100 percent scale factor version. Note that although the legacy asset has
// been deprecated, it might be still used by existing users.
extern const int kLastLegacyImageIndex;

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

// Returns the effective scale factor for a default user image with the
// specified index. This accounts for some default user images only being
// available in limited resolutions.
ui::ResourceScaleFactor GetAdjustedScaleFactorForDefaultImage(
    int index,
    ui::ResourceScaleFactor scale_factor);

// Returns the URL to a default user image with the specified index. If the
// index is invalid, returns the default user image for index 0 (anonymous
// avatar image). The resource URL will take into account the `scale_factor`
// requested and the available resolutions for the image index. The image
// resolution default is 2x except for images without available 2x versions.
GURL GetDefaultImageUrl(int index,
                        ui::ResourceScaleFactor scale_factor = ui::k200Percent);

// Returns ID of default user image with specified index.
int GetDefaultImageResourceId(int index);

// Returns bitmap of the stub default user image.
const gfx::ImageSkia& GetStubDefaultImage();

// Returns a random default image index.
int GetRandomDefaultImageIndex();

// Returns true if `index` is a valid default image index.
bool IsValidIndex(int index);

// Returns true if `index` is a in the current set of default images.
bool IsInCurrentImageSet(int index);

// Returns the default user image at the specified `index` with the
// specified `scale_factor` when possible (otherwise, with the max
// available resolution).
DefaultUserImage GetDefaultUserImage(
    int index,
    ui::ResourceScaleFactor scale_factor = ui::k200Percent);

// Returns a vector of current |DefaultUserImage|.
std::vector<DefaultUserImage> GetCurrentImageSet();

base::Value::List GetCurrentImageSetAsListValue();

// Returns the source info of the default user image with specified index.
// Returns nullopt if there is no source info.
// Only a small number of deprecated user images have associated
// |DeprecatedSourceInfo|, and none of them can be selected by users now.
std::optional<DeprecatedSourceInfo> GetDeprecatedDefaultImageSourceInfo(
    size_t index);

}  // namespace ash::default_user_image

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
