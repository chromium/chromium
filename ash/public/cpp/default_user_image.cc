// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/default_user_image.h"

#include <string>

#include "url/gurl.h"

namespace ash::default_user_image {

// DeprecatedSourceInfo
DeprecatedSourceInfo::DeprecatedSourceInfo() = default;

DeprecatedSourceInfo::DeprecatedSourceInfo(std::u16string author, GURL website)
    : author(std::move(author)), website(std::move(website)) {}

DeprecatedSourceInfo::DeprecatedSourceInfo(DeprecatedSourceInfo&&) = default;
DeprecatedSourceInfo& DeprecatedSourceInfo::operator=(DeprecatedSourceInfo&&) =
    default;

DeprecatedSourceInfo::~DeprecatedSourceInfo() = default;

// DefaultUserImage
DefaultUserImage::DefaultUserImage() = default;

DefaultUserImage::DefaultUserImage(
    int index,
    std::u16string title,
    GURL url,
    std::optional<DeprecatedSourceInfo> source_info)
    : index(index),
      title(std::move(title)),
      url(std::move(url)),
      source_info(std::move(source_info)) {}

DefaultUserImage::DefaultUserImage(DefaultUserImage&&) = default;
DefaultUserImage& DefaultUserImage::operator=(DefaultUserImage&&) = default;

DefaultUserImage::~DefaultUserImage() = default;

}  // namespace ash::default_user_image
