// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_install_with_prompt.h"

#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

WebstoreInstallWithPrompt::WebstoreInstallWithPrompt(
    const std::string& webstore_item_id,
    Profile* profile,
    const Callback& callback)
    : WebstoreStandaloneInstaller(webstore_item_id, profile, callback),
      show_post_install_ui_(true),
      dummy_web_contents_(
          WebContents::Create(WebContents::CreateParams(profile))),
      parent_window_(NULL) {
  set_install_source(WebstoreInstaller::INSTALL_SOURCE_OTHER);
}

WebstoreInstallWithPrompt::WebstoreInstallWithPrompt(
    const std::string& webstore_item_id,
    Profile* profile,
    gfx::NativeWindow parent_window,
    const Callback& callback)
    : WebstoreStandaloneInstaller(webstore_item_id, profile, callback),
      show_post_install_ui_(true),
      dummy_web_contents_(
          WebContents::Create(WebContents::CreateParams(profile))),
      parent_window_(parent_window) {
  if (parent_window_)
    parent_window_tracker_ = NativeWindowTracker::Create(parent_window);
  set_install_source(WebstoreInstaller::INSTALL_SOURCE_OTHER);
}

WebstoreInstallWithPrompt::~WebstoreInstallWithPrompt() {
}

bool WebstoreInstallWithPrompt::CheckRequestorAlive() const {
  if (!parent_window_) {
    // Assume the requestor is always alive if |parent_window_| is null.
    return true;
  }
  return !parent_window_tracker_->WasNativeWindowClosed();
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
WebstoreInstallWithPrompt::CreateInstallPrompt() const {
  return std::make_unique<ExtensionInstallPrompt::Prompt>(
      ExtensionInstallPrompt::INSTALL_PROMPT);
}

std::unique_ptr<ExtensionInstallPrompt>
WebstoreInstallWithPrompt::CreateInstallUI() {
  // Create an ExtensionInstallPrompt. If the parent window is NULL, the dialog
  // will be placed in the middle of the screen.
  return std::make_unique<ExtensionInstallPrompt>(profile(), parent_window_);
}

bool WebstoreInstallWithPrompt::ShouldShowPostInstallUI() const {
  return show_post_install_ui_;
}

bool WebstoreInstallWithPrompt::ShouldShowAppInstalledBubble() const {
  return false;
}

WebContents* WebstoreInstallWithPrompt::GetWebContents() const {
  return dummy_web_contents_.get();
}

}  // namespace extensions
