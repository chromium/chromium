// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_SETTER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_SETTER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/shell_integration.h"

namespace default_browser {

using DefaultBrowserState = shell_integration::DefaultWebClientState;

using DefaultBrowserSetterCompletionCallback =
    base::OnceCallback<void(DefaultBrowserState)>;

enum class DefaultBrowserSetterType {
  // This setter utilizes `shell_integration` to set the Default Browser
  // directly (Mac and Linux) or by opening the Settings Panel UI and navigating
  // to the application page (Windows).
  kShellIntegration = 0,

  // On Mac and Linux, this setter will set the Default Browser directly.
  // On Windows, this setter open the Settings Panel UI with a visual guided
  // page to assist the user to set the default browser.
  kVisualGuide = 1,
  kMaxValue = kVisualGuide,
};

class DefaultBrowserSetter {
 public:
  virtual ~DefaultBrowserSetter() = default;

  // Returns the setter's type for UI configuration.
  virtual DefaultBrowserSetterType GetType() const = 0;

  // Asynchronously starts the process of setting the browser as default.
  // Accepts a callback for success/failure handling.
  virtual void Execute(DefaultBrowserSetterCompletionCallback on_complete) = 0;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_SETTER_H_
