// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEFAULT_USER_IMAGE_H_
#define ASH_PUBLIC_CPP_DEFAULT_USER_IMAGE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash::default_user_image {

// Only relevant for a few deprecated avatar images that users can no longer
// select.
struct ASH_PUBLIC_EXPORT DeprecatedSourceInfo {
 public:
  DeprecatedSourceInfo();
  DeprecatedSourceInfo(std::u16string author, GURL website);

  DeprecatedSourceInfo(DeprecatedSourceInfo&&);
  DeprecatedSourceInfo& operator=(DeprecatedSourceInfo&&);

  DeprecatedSourceInfo(const DeprecatedSourceInfo&) = delete;
  DeprecatedSourceInfo& operator=(const DeprecatedSourceInfo&) = delete;

  ~DeprecatedSourceInfo();

  std::u16string author;
  GURL website;
};

struct ASH_PUBLIC_EXPORT DefaultUserImage {
 public:
  DefaultUserImage();
  DefaultUserImage(int index,
                   std::u16string title,
                   GURL url,
                   absl::optional<DeprecatedSourceInfo> source_info);

  DefaultUserImage(DefaultUserImage&&);
  DefaultUserImage& operator=(DefaultUserImage&&);

  DefaultUserImage(const DefaultUserImage&) = delete;
  DefaultUserImage& operator=(const DefaultUserImage&) = delete;

  ~DefaultUserImage();

  int index;
  std::u16string title;
  // If the AvatarsCloudMigration feature flag is turned on, this is the gstatic
  // URL of the image. Otherwise, this is the local resources URL of the image.
  GURL url;
  // Deprecated. Only used for older avatar images that users can no longer
  // select.
  absl::optional<DeprecatedSourceInfo> source_info;
};

}  // namespace ash::default_user_image

#endif  // ASH_PUBLIC_CPP_DEFAULT_USER_IMAGE_H_
