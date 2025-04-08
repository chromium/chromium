// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_

#include "components/sessions/core/session_id.h"

namespace ash {

// Abstraction of the `Browser` class from chrome/browser/ui/browser.h for use
// by ChromeOS feature code. See README.md.
class BrowserDelegate {
 public:
  // Returns the browser's unique ID for the current session.
  virtual SessionID GetSessionID() const = 0;

 protected:
  ~BrowserDelegate() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_
