// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/webstore_widget_private/app_installer.h"

#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {
const char kWebContentsDestroyedError[] = "WebContents is destroyed.";
}  // namespace

namespace webstore_widget {

class AppInstaller::WebContentsObserver : public content::WebContentsObserver {
 public:
  WebContentsObserver() = delete;

  WebContentsObserver(content::WebContents* web_contents, AppInstaller* parent)
      : content::WebContentsObserver(web_contents), parent_(parent) {}

  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;

 protected:
  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    parent_->OnWebContentsDestroyed(web_contents());
  }

 private:
  AppInstaller* parent_;
};

AppInstaller::AppInstaller(content::WebContents* web_contents,
                           const std::string& item_id,
                           Profile* profile,
                           bool silent_installation,
                           Callback callback)
    : extensions::WebstoreStandaloneInstaller(item_id,
                                              profile,
                                              std::move(callback)),
      silent_installation_(silent_installation),
      web_contents_(web_contents),
      web_contents_observer_(new WebContentsObserver(web_contents, this)) {
  DCHECK(web_contents_);
}

AppInstaller::~AppInstaller() {}

bool AppInstaller::CheckRequestorAlive() const {
  // The tab may have gone away - cancel installation in that case.
  return web_contents_ != nullptr;
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
AppInstaller::CreateInstallPrompt() const {
  if (silent_installation_)
    return nullptr;

  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::WEBSTORE_WIDGET_PROMPT));

  prompt->SetWebstoreData(localized_user_count(), show_user_count(),
                          average_rating(), rating_count());
  return prompt;
}

bool AppInstaller::ShouldShowPostInstallUI() const {
  return false;
}

bool AppInstaller::ShouldShowAppInstalledBubble() const {
  return false;
}

content::WebContents* AppInstaller::GetWebContents() const {
  return web_contents_;
}

void AppInstaller::OnWebContentsDestroyed(content::WebContents* web_contents) {
  RunCallback(false, kWebContentsDestroyedError,
              extensions::webstore_install::OTHER_ERROR);
  AbortInstall();
}

}  // namespace webstore_widget
