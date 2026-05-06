// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/callback_list.h"

namespace dictation {

class SessionController;
class SessionUi;
class StreamProvider;

// Interface for a Profile-level delegate for the dictation session. The
// SessionControllerDelegate class is responsible creation of the concrete
// implementations of various objects
class SessionControllerDelegate {
 public:
  virtual ~SessionControllerDelegate() = default;

  virtual std::unique_ptr<StreamProvider> CreateStreamProvider(
      SessionController& controller) const = 0;
  virtual std::unique_ptr<SessionUi> CreateUi(
      SessionController& controller) const = 0;

  // Ends the current session. No-op if there is no existing session. Note: this
  // may destroy the session controller synchronously.
  virtual void EndSession() = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_DELEGATE_H_
