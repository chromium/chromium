// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_SHELL_INTEGRATION_DEFAULT_BROWSER_SETTER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_SHELL_INTEGRATION_DEFAULT_BROWSER_SETTER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/default_browser/default_browser_setter.h"

namespace shell_integration {
class DefaultBrowserWorker;
}

namespace default_browser {

// This setter uses the methods and classes defined in `c/b/shell_integration`
// to set Chrome as default browser.
// For Mac and Linux, this sets the default browser directly (non-interactive).
// For Windows, this opens the Settings Panel UI and navigates to the
// application page.
class ShellIntegrationDefaultBrowserSetter : public DefaultBrowserSetter {
 public:
  ShellIntegrationDefaultBrowserSetter();
  ~ShellIntegrationDefaultBrowserSetter() override;

  ShellIntegrationDefaultBrowserSetter(
      const ShellIntegrationDefaultBrowserSetter&) = delete;
  ShellIntegrationDefaultBrowserSetter& operator=(
      const ShellIntegrationDefaultBrowserSetter&) = delete;

  DefaultBrowserSetterType GetType() const override;
  void Execute(DefaultBrowserSetterCompletionCallback on_complete) override;

 private:
  void OnComplete(DefaultBrowserState default_browser_state);

  // Stores the callback which will be executed once the worker has finished
  // executing.
  DefaultBrowserSetterCompletionCallback on_complete_callback_;
  // Worker instance internally decides which method to use for setting the
  // default browser. Resets upon reporting completion.
  scoped_refptr<shell_integration::DefaultBrowserWorker> worker_;

  base::WeakPtrFactory<ShellIntegrationDefaultBrowserSetter> weak_ptr_factory_{
      this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_SHELL_INTEGRATION_DEFAULT_BROWSER_SETTER_H_
