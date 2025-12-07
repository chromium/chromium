// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_

#include "chrome/browser/extensions/extension_install_prompt.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class ExtensionInstallPromptShowParams;

namespace extensions {
class Extension;

// An interface for error to show the user an extension has been externally
// installed.
class ExternalInstallError {
 public:
  // The possible types of errors to show. A menu alert adds a menu item to the
  // wrench, which spawns an extension install dialog when clicked. The bubble
  // alert also adds an item, but spawns a bubble instead (less invasive and
  // easier to dismiss).
  enum AlertType { BUBBLE_ALERT, MENU_ALERT };

  virtual ~ExternalInstallError() = default;

  virtual void OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload payload) = 0;
  virtual void DidOpenBubbleView() = 0;
  virtual void DidCloseBubbleView() = 0;

  // Returns the associated extension, or NULL.
  virtual const Extension* GetExtension() const = 0;

  // Returns the associated extension id.
  virtual const ExtensionId& extension_id() const = 0;

  // Returns the alert type of error UI.
  virtual AlertType alert_type() const = 0;

  virtual ExtensionInstallPrompt::Prompt* GetPromptForTesting() const = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_
