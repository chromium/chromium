// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_WEBSTORE_WIDGET_PRIVATE_APP_INSTALLER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_WEBSTORE_WIDGET_PRIVATE_APP_INSTALLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"

namespace content {
class WebContents;
}  // namespace content

namespace webstore_widget {

// This class is used for installing apps and extensions suggested from the
// Chrome Web Store Gallery widget.
class AppInstaller : public extensions::WebstoreStandaloneInstaller {
 public:
  AppInstaller() = delete;

  AppInstaller(content::WebContents* web_contents,
               const std::string& item_id,
               Profile* profile,
               bool silent_installation,
               Callback callback);

  AppInstaller(const AppInstaller&) = delete;
  AppInstaller& operator=(const AppInstaller&) = delete;

 protected:
  friend class base::RefCountedThreadSafe<AppInstaller>;

  ~AppInstaller() override;

  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // WebstoreStandaloneInstaller implementation.
  bool CheckRequestorAlive() const override;
  bool ShouldShowPostInstallUI() const override;
  bool ShouldShowAppInstalledBubble() const override;
  content::WebContents* GetWebContents() const override;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const override;

 private:
  class WebContentsObserver;

  bool silent_installation_;
  content::WebContents* web_contents_;
  std::unique_ptr<WebContentsObserver> web_contents_observer_;
};

}  // namespace webstore_widget

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_WEBSTORE_WIDGET_PRIVATE_APP_INSTALLER_H_
