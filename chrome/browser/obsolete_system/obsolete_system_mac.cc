// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/mac/mac_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ObsoleteSystem {

bool IsObsoleteNowOrSoon() {
#if CHROME_VERSION_MAJOR >= 126
  return base::mac::MacOSMajorVersion() < 11;
#else
  return false;
#endif
}

std::u16string LocalizedObsoleteString() {
  return l10n_util::GetStringUTF16(IDS_MACOS_OBSOLETE);
}

bool IsEndOfTheLine() {
  // M128 is the last milestone supporting macOS 10.15.
  return CHROME_VERSION_MAJOR >= 128;
}

const char* GetLinkURL() {
  return chrome::kMacOsObsoleteURL;
}

}  // namespace ObsoleteSystem
