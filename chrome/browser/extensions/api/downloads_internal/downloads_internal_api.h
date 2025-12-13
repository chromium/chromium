// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_INTERNAL_DOWNLOADS_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_INTERNAL_DOWNLOADS_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// NOTE: The downloadsInternal API is required for the downloads API to
// function, because downloads_custom_bindings.js refers to downloadsInternal.
class DownloadsInternalDetermineFilenameFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloadsInternal.determineFilename",
                             DOWNLOADSINTERNAL_DETERMINEFILENAME)
  DownloadsInternalDetermineFilenameFunction();

  DownloadsInternalDetermineFilenameFunction(
      const DownloadsInternalDetermineFilenameFunction&) = delete;
  DownloadsInternalDetermineFilenameFunction& operator=(
      const DownloadsInternalDetermineFilenameFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsInternalDetermineFilenameFunction() override;
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_INTERNAL_DOWNLOADS_INTERNAL_API_H_
