// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/cpu.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool IsObsoleteOsVersion() {
  return base::win::GetVersion() < base::win::Version::WIN7;
}

bool IsObsoleteCpu() {
#if defined(ARCH_CPU_X86_FAMILY)
  return !base::CPU().has_sse3();
#else
  return false;
#endif
}

}  // namespace

// static
bool ObsoleteSystem::IsObsoleteNowOrSoon() {
  return IsObsoleteCpu() || IsObsoleteOsVersion();
}

// static
base::string16 ObsoleteSystem::LocalizedObsoleteString() {
  // We check for an obsolete CPU first so that we don't nudge users through
  // an OS upgrade, only to find out that they need a new computer anyway.
  return IsObsoleteCpu()
             ? l10n_util::GetStringUTF16(IDS_CPU_X86_SSE2_OBSOLETE_SOON)
             : l10n_util::GetStringUTF16(IDS_WIN_XP_VISTA_OBSOLETE);
}

// static
bool ObsoleteSystem::IsEndOfTheLine() {
  return IsObsoleteCpu() ? CHROME_VERSION_MAJOR >= 88 : true;
}

// static
const char* ObsoleteSystem::GetLinkURL() {
  return IsObsoleteCpu() ? chrome::kCpuX86Sse2ObsoleteURL
                         : chrome::kWindowsXPVistaDeprecationURL;
}
