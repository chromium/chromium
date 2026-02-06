// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_CHROME_CUSTOM_ICON_CATALOG_H_
#define ASH_CONSTANTS_CHROME_CUSTOM_ICON_CATALOG_H_

namespace ash {

// Used as allow-list of icons that Chrome can add to the StatusArea (a.k.a
// StatusTray) of ChromeOS via StatusTray interface. (See `StatusTrayChromeOS`
// for details)
enum class ChromeCustomIconCatalogName {
  kNotSupported = 0,
  kGlic = 1,
  kMaxValue = kGlic
};

}  // namespace ash

#endif  // ASH_CONSTANTS_CHROME_CUSTOM_ICON_CATALOG_H_
