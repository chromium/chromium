// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_

namespace dictation {

// Interface for the UI to communicate back to the session controller.
class SessionUiDelegate {
 public:
  virtual ~SessionUiDelegate() = default;

  // Called when the session end has been requested via the UI.
  virtual void RequestEndSession() = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_
