// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_H_

namespace dictation {

// Interface for the view controller of browser-level UI behavior.
class SessionUi {
 public:
  virtual ~SessionUi() = default;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_H_
