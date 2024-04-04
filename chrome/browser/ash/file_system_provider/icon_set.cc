// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/icon_set.h"

namespace ash::file_system_provider {

IconSet::IconSet() = default;
IconSet::IconSet(const IconSet& icon_set) = default;
IconSet::~IconSet() = default;

void IconSet::SetIcon(IconSize size, const GURL& icon_url) {
  icons_[size] = icon_url;
}

bool IconSet::HasIcon(IconSize size) const {
  return icons_.find(size) != icons_.end();
}

const GURL& IconSet::GetIcon(IconSize size) const {
  const auto it = icons_.find(size);
  if (it == icons_.end())
    return GURL::EmptyGURL();

  return it->second;
}

bool IconSet::operator==(const IconSet& other) const {
  // Check each IconSize exists (and is equal) in both or neither.
  for (IconSet::IconSize size = IconSet::IconSize::SIZE_16x16;
       size <= IconSet::IconSize::kMaxValue; ++(int&)size) {
    if (HasIcon(size) != other.HasIcon(size)) {
      return false;
    }
    if (HasIcon(size) && GetIcon(size) != other.GetIcon(size)) {
      return false;
    }
  }
  return true;
}

}  // namespace ash::file_system_provider
