// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_ANDROID_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExternalInstallManager;

// Desktop Android implementation of ExternalInstallError.
// TODO(crbug.com/405391110): The UI for showing errors for external installed
// extensions need to be implemented on desktop android. In addition, web
// store integration will be made available on desktop android later. This is
// just a stub to make the code compiled on desktop android for now.
class ExternalInstallErrorAndroid : public ExternalInstallError {
 public:
  ExternalInstallErrorAndroid(content::BrowserContext* browser_context,
                              const std::string& extension_id,
                              AlertType error_type,
                              ExternalInstallManager* manager);

  ExternalInstallErrorAndroid(const ExternalInstallErrorAndroid&) = delete;
  ExternalInstallErrorAndroid& operator=(const ExternalInstallErrorAndroid&) =
      delete;

  ~ExternalInstallErrorAndroid() override;

  // ExternalInstallError:
  void OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload payload) override;
  void DidOpenBubbleView() override;
  void DidCloseBubbleView() override;
  const Extension* GetExtension() const override;
  const ExtensionId& extension_id() const override;
  ExternalInstallError::AlertType alert_type() const override;
  ExtensionInstallPrompt::Prompt* GetPromptForTesting() const override;

 private:
  // The associated BrowserContext.
  raw_ptr<content::BrowserContext> browser_context_;

  // The id of the external extension.
  ExtensionId extension_id_;

  // The type of alert to show the user.
  AlertType alert_type_;

  // The owning ExternalInstallManager.
  raw_ptr<ExternalInstallManager> manager_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_ANDROID_H_
