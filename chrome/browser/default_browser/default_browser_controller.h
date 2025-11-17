// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
#include "chrome/browser/shell_integration.h"

namespace default_browser {

using DefaultBrowserControllerCompletionCallback =
    base::OnceCallback<void(DefaultBrowserState)>;

// Denotes different UI entrypoints to the default browser setting flows.
enum class DefaultBrowserEntrypointType {
  // Infobar shown during startup when Chrome is not the default browser.
  kStartupInfobar = 0,
  // Default browser Settings page UI.
  kSettingsPage = 1,
  kMaxValue = kSettingsPage
};

// DefaultBrowserController acts a bridge between UI and the setter, is
// responsible for managing the setter based on user input, and recording
// metrics.
class DefaultBrowserController {
 public:
  DefaultBrowserController(std::unique_ptr<DefaultBrowserSetter> setter,
                           DefaultBrowserEntrypointType ui_entrypoint);
  ~DefaultBrowserController();

  DefaultBrowserController(const DefaultBrowserController&) = delete;
  DefaultBrowserController& operator=(const DefaultBrowserController&) = delete;

  DefaultBrowserSetterType GetSetterType() const;

  // Called by UI based on user interactions.
  void OnShown();
  void OnAccepted(
      DefaultBrowserControllerCompletionCallback on_setter_completion_callback);
  void OnIgnored();
  void OnDismissed();

 private:
  void OnSetterExecutionComplete(DefaultBrowserState default_browser_state);

  // Stores the callback that gets triggered when setter completes execution.
  DefaultBrowserControllerCompletionCallback completion_callback_;
  std::unique_ptr<DefaultBrowserSetter> setter_;

  base::WeakPtrFactory<DefaultBrowserController> weak_ptr_factory_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_CONTROLLER_H_
