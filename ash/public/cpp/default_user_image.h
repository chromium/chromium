// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEFAULT_USER_IMAGE_H_
#define ASH_PUBLIC_CPP_DEFAULT_USER_IMAGE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "url/gurl.h"

namespace ash {
namespace default_user_image {

struct ASH_PUBLIC_EXPORT DefaultUserImage {
  int index;
  std::u16string title;
  GURL url;
};

}  // namespace default_user_image
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DEFAULT_USER_IMAGE_H_
