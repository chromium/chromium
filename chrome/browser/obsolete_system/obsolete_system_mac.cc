// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/cpu.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool IsObsoleteOsVersion() {
  // Use base::SysInfo::OperatingSystemVersionNumbers() here rather than the
  // preferred base::mac::IsOS*() function because the IsOS functions for
  // obsolete system versions are removed to help prevent obsolete code from
  // existing in the Chromium codebase.
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);

  return ((major < 10) || (major == 10 && minor <= 10)) &&
         base::FeatureList::IsEnabled(features::kShow10_10ObsoleteInfobar);
}

}  // namespace

// static
bool ObsoleteSystem::IsObsoleteNowOrSoon() {
  return IsObsoleteOsVersion();
}

// static
std::u16string ObsoleteSystem::LocalizedObsoleteString() {
  return l10n_util::GetStringUTF16(IDS_MAC_10_10_OBSOLETE_SOON);
}

// static
bool ObsoleteSystem::IsEndOfTheLine() {
  return true;
}

// static
const char* ObsoleteSystem::GetLinkURL() {
  return chrome::kMac10_10_ObsoleteURL;
}
