// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_H_

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace screen_ai {

// Gives the Screen AI service handlers access to the Screen AI library,
// without them knowing how the library is downloaded and installed.
class ScreenAILibraryProvider {
 public:
  // Returns the path of the library binary, or an empty path if the library is
  // not available on the device yet.
  virtual base::FilePath GetBinaryPath() const = 0;

  // Records that the library is used now, so that it stays installed.
  virtual void RecordUsage() = 0;

  // Makes sure that the library is on the device. Downloads it if needed, also
  // when an earlier download has failed. Runs `callback` with true when the
  // library is available, or with false when it could not be downloaded. This
  // can happen before this function returns.
  //
  // `callback` can call this function again, for example to try once more
  // after a failure. That request waits for a new download.
  //
  // `callback` does not run after the returned subscription is destroyed. The
  // download continues in that case.
  virtual base::CallbackListSubscription EnsureLibraryAvailable(
      base::OnceCallback<void(bool)> callback) = 0;

 protected:
  // The users of a provider do not own it.
  virtual ~ScreenAILibraryProvider() = default;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_H_
