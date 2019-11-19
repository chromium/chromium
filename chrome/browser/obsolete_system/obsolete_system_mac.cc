// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/system/sys_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
bool ObsoleteSystem::IsObsoleteNowOrSoon() {
  // Use base::SysInfo::OperatingSystemVersionNumbers() here rather than the
  // preferred base::mac::IsOS*() function because the IsOS functions for
  // obsolete system versions are removed to help prevent obsolete code from
  // existing in the Chromium codebase.
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);

  return ((major < 10) || (major == 10 && minor <= 9)) &&
         base::FeatureList::IsEnabled(features::kShow10_9ObsoleteInfobar);
}

// static
base::string16 ObsoleteSystem::LocalizedObsoleteString() {
  return l10n_util::GetStringUTF16(IDS_MAC_10_9_OBSOLETE_NOW);
}

// static
bool ObsoleteSystem::IsEndOfTheLine() {
  return true;
}

// static
const char* ObsoleteSystem::GetLinkURL() {
  return chrome::kMac10_9_ObsoleteURL;
}
