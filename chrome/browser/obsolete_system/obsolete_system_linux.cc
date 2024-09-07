// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/linux_util.h"
#include "base/strings/string_util.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"

const char kLinuxObsoleteUrl[] =
    "https://support.google.com/chrome/?p=unsupported_linux";

// This list contains the most recently obsoleted distros.
const char* const kObsoleteDistros[] = {
    // Prevent clang-format from adding multiple entries per line.
    // clang-format off
    "Debian 8",
    "Debian 9",
    "Debian 10",
    "Fedora 30",
    "Fedora 31",
    "Fedora 32",
    "Fedora 33",
    "Fedora 34",
    "Fedora 35",
    "Fedora 36",
    "Fedora 37",
    "Fedora 38",
    "Ubuntu 14.04",
    "Ubuntu 16.04",
    "Ubuntu 18.04",
    "openSUSE Leap 15.0",
    "openSUSE Leap 15.1",
    "openSUSE Leap 15.2",
    "openSUSE Leap 15.3",
    "openSUSE Leap 15.4",
    // clang-format on
};

namespace ObsoleteSystem {

bool IsObsoleteNowOrSoon() {
  auto distro = base::GetLinuxDistro();
  for (const char* obsolete : kObsoleteDistros) {
    if (base::StartsWith(distro, obsolete)) {
      return true;
    }
  }
  return false;
}

std::u16string LocalizedObsoleteString() {
  return l10n_util::GetStringUTF16(IDS_LINUX_OBSOLETE);
}

bool IsEndOfTheLine() {
  return false;
}

const char* GetLinkURL() {
  return kLinuxObsoleteUrl;
}

}  // namespace ObsoleteSystem
