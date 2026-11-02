// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/cpu.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Obsolete-system checks get the system version from kernel32.dll's version, to
// avoid getting an incorrect version reported by App Compatibility mode. This
// prevents obsolete-system warnings from appearing when Chrome is run in
// compatibility mode on modern versions of Windows.
base::win::Version GetRealOSVersion() {
  return base::win::OSInfo::Kernel32Version();
}

bool IsObsoleteOsVersion() {
  return GetRealOSVersion() < base::win::Version::WIN10;
}

}  // namespace

namespace ObsoleteSystem {

bool IsObsoleteNowOrSoon() {
  return IsObsoleteOsVersion();
}

std::u16string LocalizedObsoleteString() {
  const auto version = GetRealOSVersion();
  if (version == base::win::Version::WIN7) {
    return l10n_util::GetStringUTF16(IDS_WIN_7_OBSOLETE);
  }
  if (version == base::win::Version::WIN8) {
    return l10n_util::GetStringUTF16(IDS_WIN_8_OBSOLETE);
  }
  if (version == base::win::Version::WIN8_1) {
    return l10n_util::GetStringUTF16(IDS_WIN_8_1_OBSOLETE);
  }
  return l10n_util::GetStringUTF16(IDS_WIN_XP_VISTA_OBSOLETE);
}

bool IsEndOfTheLine() {
  // M109 was the last milestone to support Win 7/8/8.1, the last deprecated
  // Windows version. Future deprecations should update this to the last
  // milestone that supports the soon-to-be-deprecated Windows version.
  return CHROME_VERSION_MAJOR >= 109;
}

const char* GetLinkURL() {
  const auto version = GetRealOSVersion();
  if (version < base::win::Version::WIN7) {
    return chrome::kWindowsXPVistaDeprecationURL;
  }
  return chrome::kWindows78DeprecationURL;
}

}  // namespace ObsoleteSystem
