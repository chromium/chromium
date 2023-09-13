// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/system/sys_info.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum class Obsoleteness {
  MacOS1013Obsolete,
  MacOS1014Obsolete,
  NotObsolete,
};

Obsoleteness OsObsoleteness() {
#if CHROME_VERSION_MAJOR >= 114
  // Use base::SysInfo::OperatingSystemVersionNumbers() here because
  // base::mac::MacOSVersion() didn't exist at the time that this code was
  // written. The next time this is reused for the next obsolete OS, use
  // base::mac::MacOSMajorVersion() instead.
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);

  if (major < 10 || (major == 10 && minor <= 13)) {
    return Obsoleteness::MacOS1013Obsolete;
  }

  if (major == 10 && minor == 14) {
    return Obsoleteness::MacOS1014Obsolete;
  }

  return Obsoleteness::NotObsolete;
#else
  return Obsoleteness::NotObsolete;
#endif
}

}  // namespace

namespace ObsoleteSystem {

bool IsObsoleteNowOrSoon() {
  return OsObsoleteness() != Obsoleteness::NotObsolete;
}

std::u16string LocalizedObsoleteString() {
  switch (OsObsoleteness()) {
    case Obsoleteness::MacOS1013Obsolete:
      return l10n_util::GetStringUTF16(IDS_MAC_10_13_OBSOLETE);
    case Obsoleteness::MacOS1014Obsolete:
      return l10n_util::GetStringUTF16(IDS_MAC_10_14_OBSOLETE);
    default:
      return std::u16string();
  }
}

bool IsEndOfTheLine() {
  // M116 is the last milestone supporting macOS 10.13 and macOS 10.14.
  return CHROME_VERSION_MAJOR >= 116;
}

const char* GetLinkURL() {
  return chrome::kMacOsObsoleteURL;
}

}  // namespace ObsoleteSystem
