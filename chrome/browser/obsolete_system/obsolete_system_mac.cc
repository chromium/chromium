// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/mac/mac_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"

// Chromium 138 will be the last mstone to support macOS 11.
constexpr int kLastMStoneWithSupport = 138;
constexpr int kMacOSReleaseBeingObsoleted = 11;

namespace ObsoleteSystem {

bool IsObsoleteNowOrSoon() {
  // Warn for the last three mstones.
  if (CHROME_VERSION_MAJOR >= kLastMStoneWithSupport - 2) {
    return base::mac::MacOSMajorVersion() <= kMacOSReleaseBeingObsoleted;
  }
  return false;
}

std::u16string LocalizedObsoleteString() {
  return l10n_util::GetStringUTF16(IDS_MACOS_OBSOLETE);
}

bool IsEndOfTheLine() {
  return CHROME_VERSION_MAJOR >= kLastMStoneWithSupport;
}

const char* GetLinkURL() {
  return chrome::kMacOsObsoleteURL;
}

}  // namespace ObsoleteSystem
