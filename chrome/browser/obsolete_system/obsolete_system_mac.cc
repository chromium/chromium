// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/system/sys_info.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum class Obsoleteness {
  MacOS1011Obsolete,
  MacOS1012Obsolete,
  NotObsolete,
};

Obsoleteness OsObsoleteness() {
  // Use base::SysInfo::OperatingSystemVersionNumbers() here rather than the
  // preferred base::mac::IsOS*() function because the IsOS functions for
  // obsolete system versions are removed to help prevent obsolete code from
  // existing in the Chromium codebase.
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);

  if (major < 10 || (major == 10 && minor <= 11))
    return Obsoleteness::MacOS1011Obsolete;

  if (major == 10 && minor == 12)
    return Obsoleteness::MacOS1012Obsolete;

  return Obsoleteness::NotObsolete;
}

}  // namespace

namespace ObsoleteSystem {

bool IsObsoleteNowOrSoon() {
  return OsObsoleteness() != Obsoleteness::NotObsolete;
}

std::u16string LocalizedObsoleteString() {
  switch (OsObsoleteness()) {
    case Obsoleteness::MacOS1011Obsolete:
      return l10n_util::GetStringUTF16(IDS_MAC_10_11_OBSOLETE);
    case Obsoleteness::MacOS1012Obsolete:
      return l10n_util::GetStringUTF16(IDS_MAC_10_12_OBSOLETE);
    default:
      return std::u16string();
  }
}

bool IsEndOfTheLine() {
  return CHROME_VERSION_MAJOR >= 103;
}

const char* GetLinkURL() {
  return chrome::kMacOsObsoleteURL;
}

}  // namespace ObsoleteSystem
