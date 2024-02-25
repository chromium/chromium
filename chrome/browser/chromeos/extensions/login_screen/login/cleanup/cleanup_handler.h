// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_HANDLER_H_

#include <optional>

#include "base/functional/callback.h"

namespace chromeos {

// A handler which is owned by `CleanupManager`. Each `CleanupHandler` is
// in charge of a separate cleanup step.
class CleanupHandler {
 public:
  using CleanupHandlerCallback =
      base::OnceCallback<void(const std::optional<std::string>& error_message)>;
  // `callback` is called after handler has finished its cleanup step.
  // `callback` must be called exactly once or there will be a memory leak. The
  // handler can assume that `CleanupManager` will not call `Cleanup` before
  // the current cleanup step has finished.
  virtual void Cleanup(CleanupHandlerCallback callback) = 0;

  virtual ~CleanupHandler() = default;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_HANDLER_H_
