// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_H_

#include "base/files/file_path.h"

namespace screen_ai {

// Gives the Screen AI service handlers access to the Screen AI library,
// without them knowing how the library is downloaded and installed.
class ScreenAILibraryProvider {
 public:
  virtual ~ScreenAILibraryProvider() = default;

  // Returns the path of the library binary, or an empty path if the library is
  // not available on the device yet.
  virtual base::FilePath GetBinaryPath() const = 0;

  // Records that the library is used now, so that it stays installed.
  virtual void SetLastUsageTime() = 0;

  // Triggers the download of the library, if it is not downloaded and no
  // download is in progress.
  virtual void Download() = 0;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_H_
