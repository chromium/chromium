// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ICON_SET_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ICON_SET_H_

#include <map>

#include "url/gurl.h"

namespace ash::file_system_provider {

// Holds urls to icons with multiple dimensions.
// TODO(mtomasz): Move this to chrome/browser/ash so it can be reused
// by other components.
class IconSet {
 public:
  enum class IconSize {
    SIZE_16x16,
    SIZE_32x32,
    kMaxValue = SIZE_32x32,
  };

  IconSet();
  IconSet(const IconSet& icon_set);

  ~IconSet();

  // Sets an icon url. If already set, it will be overriden.
  void SetIcon(IconSize size, const GURL& icon_url);

  // Checks if the set contains an icon of the specified size.
  bool HasIcon(IconSize size) const;

  // Gets an icon URL for the exact size. If not specified, then an invalid
  // URL will be returned.
  const GURL& GetIcon(IconSize size) const;

  bool operator==(const IconSet& other) const;

 private:
  std::map<IconSize, GURL> icons_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ICON_SET_H_
