// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_TEST_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_TEST_UTIL_H_

#include <vector>

namespace gfx {
class ImageSkia;
}

namespace apps {

struct IconValue;

constexpr int kSizeInDip = 64;

void EnsureRepresentationsLoaded(gfx::ImageSkia& output_image_skia);

void LoadDefaultIcon(gfx::ImageSkia& output_image_skia);

void VerifyIcon(const gfx::ImageSkia& src, const gfx::ImageSkia& dst);

void VerifyCompressedIcon(const std::vector<uint8_t>& src_data,
                          const apps::IconValue& icon);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_TEST_UTIL_H_
