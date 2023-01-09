// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_REINSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_REINSTALLER_H_

#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "content/public/browser/web_contents_observer.h"

namespace extensions {

// Reinstalls an extension from the webstore. This will first prompt the user if
// they want to reinstall (using the verbase "Repair", since this is our action
// for repairing corrupted extensions), and, if the user agrees, will uninstall
// the extension and reinstall it directly from the webstore.
class WebstoreReinstaller : public WebstoreStandaloneInstaller,
                            public content::WebContentsObserver {
 public:
  WebstoreReinstaller(content::WebContents* web_contents,
                      const std::string& extension_id,
                      WebstoreStandaloneInstaller::Callback callback);

  // Begin the reinstall process. |callback| (from the constructor) will be
  // called upon completion.
  void BeginReinstall();

 private:
  ~WebstoreReinstaller() override;

  // WebstoreStandaloneInstaller:
  bool CheckRequestorAlive() const override;
  bool ShouldShowPostInstallUI() const override;
  content::WebContents* GetWebContents() const override;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const override;
  void OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload payload) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_REINSTALLER_H_
