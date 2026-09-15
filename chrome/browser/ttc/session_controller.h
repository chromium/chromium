// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_TTC_SESSION_CONTROLLER_H_

#include "chrome/browser/ttc/core/page_context.h"

namespace ttc {

class Conversation;

// High-level lifecycle coordinator for a TTC session. Manages the lifetime
// of the session UI (SessionView) and the model interaction (Conversation).
// Agnostic of the underlying MES transport protocol.
class SessionController {
 public:
  virtual ~SessionController() = default;

  // Fetches the context of the page this session is operating on, invoking
  // `callback` with the result.
  virtual void GetPageContext(FetchCompleteCallback callback) = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_SESSION_CONTROLLER_H_
