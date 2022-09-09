// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "extensions/common/extension_set.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// The controller for the ExtensionErrorUI. This examines extensions for any
// blocklisted or external extensions in order to notify the user with an error.
// On acceptance, this will acknowledge the extensions.
class ExtensionErrorController : public ExtensionErrorUI::Delegate {
 public:
  typedef ExtensionErrorUI* (*UICreateMethod)(ExtensionErrorUI::Delegate*);

  ExtensionErrorController(content::BrowserContext* context, bool is_first_run);

  ExtensionErrorController(const ExtensionErrorController&) = delete;
  ExtensionErrorController& operator=(const ExtensionErrorController&) = delete;

  virtual ~ExtensionErrorController();

  void ShowErrorIfNeeded();

  // Set the factory method for creating a new ExtensionErrorUI.
  static void SetUICreateMethodForTesting(UICreateMethod method);

 private:
  // ExtensionErrorUI::Delegate implementation:
  content::BrowserContext* GetContext() override;
  const ExtensionSet& GetBlocklistedExtensions() override;
  void OnAlertDetails() override;
  void OnAlertAccept() override;
  void OnAlertClosed() override;

  // Find any extensions that the user should be alerted about (like blocklisted
  // extensions).
  void IdentifyAlertableExtensions();

  // The extensions that are blocklisted and need user approval.
  ExtensionSet blocklisted_extensions_;

  // The UI component of this controller.
  std::unique_ptr<ExtensionErrorUI> error_ui_;

  // The BrowserContext with which we are associated.
  raw_ptr<content::BrowserContext> browser_context_;

  // Whether or not this is the first run. If it is, we avoid noisy errors, and
  // silently acknowledge blocklisted extensions.
  bool is_first_run_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_CONTROLLER_H_
