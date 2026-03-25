// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_crx_util.h"

#include "extensions/buildflags/buildflags.h"

// This file is used on non-desktop Android where extensions are not supported.
static_assert(!BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace download_crx_util {

bool IsTrustedExtensionDownload(Profile* profile,
                                const download::DownloadItem& item) {
  // Extensions are not supported on non-desktop Android, return the safe
  // default.
  return false;
}

}  // namespace download_crx_util
