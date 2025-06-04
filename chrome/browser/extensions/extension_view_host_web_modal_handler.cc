// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_web_modal_handler.h"

#include <memory>

#include "chrome/browser/platform_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

ExtensionViewHostWebModalHandler::ExtensionViewHostWebModalHandler(
    content::WebContents* web_contents,
    gfx::NativeView view)
    : web_contents_(web_contents), view_(view) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
      ->SetDelegate(this);
}

ExtensionViewHostWebModalHandler::~ExtensionViewHostWebModalHandler() {
  auto* const manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (manager) {
    manager->SetDelegate(nullptr);
  }
  for (auto& observer : modal_dialog_host_observers_) {
    observer.OnHostDestroying();
  }
}

web_modal::WebContentsModalDialogHost*
ExtensionViewHostWebModalHandler::GetWebContentsModalDialogHost() {
  return this;
}

bool ExtensionViewHostWebModalHandler::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

gfx::NativeView ExtensionViewHostWebModalHandler::GetHostView() const {
  return view_;
}

gfx::Point ExtensionViewHostWebModalHandler::GetDialogPosition(
    const gfx::Size& size) {
  const gfx::Size view_size = web_contents_->GetViewBounds().size();
  return gfx::Rect(view_size - size).CenterPoint();
}

gfx::Size ExtensionViewHostWebModalHandler::GetMaximumDialogSize() {
  return web_contents_->GetViewBounds().size();
}

void ExtensionViewHostWebModalHandler::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.AddObserver(observer);
}

void ExtensionViewHostWebModalHandler::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.RemoveObserver(observer);
}

}  // namespace extensions
