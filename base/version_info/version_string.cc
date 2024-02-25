// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/version_string.h"

#include "base/version_info/version_info.h"

namespace version_info {

std::string GetVersionStringWithModifier(const std::string& modifier) {
  std::string current_version;
  current_version += GetVersionNumber();
#if defined(USE_UNOFFICIAL_VERSION_NUMBER)
  current_version += " (Developer Build ";
  current_version += GetLastChange();
  current_version += " ";
  current_version += GetOSType();
  current_version += ")";
#endif  // USE_UNOFFICIAL_VERSION_NUMBER
  if (!modifier.empty()) {
    current_version += " " + modifier;
  }
  return current_version;
}

}  // namespace version_info
