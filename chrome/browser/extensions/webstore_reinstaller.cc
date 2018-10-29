// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_reinstaller.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace {
const char kCouldNotUninstallExtension[] = "Failed to uninstall the extension.";
const char kTabClosed[] = "Tab was closed.";
}

WebstoreReinstaller::WebstoreReinstaller(
    content::WebContents* web_contents,
    const std::string& extension_id,
    const WebstoreStandaloneInstaller::Callback& callback)
    : WebstoreStandaloneInstaller(
          extension_id,
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          callback),
      content::WebContentsObserver(web_contents) {
  DCHECK(
      ExtensionPrefs::Get(web_contents->GetBrowserContext())
          ->HasDisableReason(extension_id, disable_reason::DISABLE_CORRUPTED));
}

WebstoreReinstaller::~WebstoreReinstaller() {
}

void WebstoreReinstaller::BeginReinstall() {
  WebstoreStandaloneInstaller::BeginInstall();
}

bool WebstoreReinstaller::CheckRequestorAlive() const {
  return web_contents() != NULL;
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
WebstoreReinstaller::CreateInstallPrompt() const {
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::REPAIR_PROMPT));
  prompt->SetWebstoreData(localized_user_count(),
                          show_user_count(),
                          average_rating(),
                          rating_count());
  return prompt;
}

bool WebstoreReinstaller::ShouldShowPostInstallUI() const {
  return false;
}

bool WebstoreReinstaller::ShouldShowAppInstalledBubble() const {
  return false;
}

content::WebContents* WebstoreReinstaller::GetWebContents() const {
  return web_contents();
}

void WebstoreReinstaller::WebContentsDestroyed() {
  // Run the callback now, because AbortInstall() doesn't do it.
  RunCallback(false, kTabClosed, webstore_install::ABORTED);
  AbortInstall();
}

void WebstoreReinstaller::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  if (result != ExtensionInstallPrompt::Result::ACCEPTED) {
    WebstoreStandaloneInstaller::OnInstallPromptDone(result);
    return;
  }

  if (!ExtensionSystem::Get(profile())->extension_service()->UninstallExtension(
          id(),
          UNINSTALL_REASON_REINSTALL,
          NULL)) {
    // Run the callback now, because AbortInstall() doesn't do it.
    RunCallback(
        false, kCouldNotUninstallExtension, webstore_install::OTHER_ERROR);
    AbortInstall();
    return;
  }
  WebstoreStandaloneInstaller::OnInstallPromptDone(result);
}

}  // namespace extensions
