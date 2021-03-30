// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ICON_STANDARDIZER_H_
#define CHROME_BROWSER_APPS_ICON_STANDARDIZER_H_

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace apps {

// Takes an icon image and returns a standardized version of that icon. This
// function consists of the following steps:
// 1. Check if the original icon is already circle shaped. If it is, then
//    return the original input icon.
// 2. Find the scale required to resize and fit the original icon inside of
//    a new circle background.
// 3. Scale down the icon and draw it over a background circle. Return the newly
//    generated icon as the standard icon.
gfx::ImageSkia CreateStandardIconImage(const gfx::ImageSkia& image);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ICON_STANDARDIZER_H_
