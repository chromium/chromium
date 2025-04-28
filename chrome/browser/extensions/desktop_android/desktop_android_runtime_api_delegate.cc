// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_runtime_api_delegate.h"

#include "base/notimplemented.h"
#include "chrome/browser/extensions/updater/extension_updater.h"

namespace extensions {

DesktopAndroidRuntimeApiDelegate::DesktopAndroidRuntimeApiDelegate(
    content::BrowserContext* context)
    : browser_context_(context) {}

DesktopAndroidRuntimeApiDelegate::~DesktopAndroidRuntimeApiDelegate() = default;

void DesktopAndroidRuntimeApiDelegate::AddUpdateObserver(
    UpdateObserver* observer) {
  registered_for_updates_ = true;
  ExtensionUpdater::Get(browser_context_)->AddObserver(observer);
}

void DesktopAndroidRuntimeApiDelegate::RemoveUpdateObserver(
    UpdateObserver* observer) {
  if (registered_for_updates_) {
    ExtensionUpdater::Get(browser_context_)->RemoveObserver(observer);
  }
}

void DesktopAndroidRuntimeApiDelegate::ReloadExtension(
    const ExtensionId& extension_id) {
  // TODO(crbug.com/373434594): Support reload.
  NOTIMPLEMENTED();
}

bool DesktopAndroidRuntimeApiDelegate::CheckForUpdates(
    const ExtensionId& extension_id,
    UpdateCheckCallback callback) {
  return false;
}

void DesktopAndroidRuntimeApiDelegate::OpenURL(const GURL& uninstall_url) {
  // TODO(crbug.com/373434594): Support opening URLs.
  NOTIMPLEMENTED();
}

bool DesktopAndroidRuntimeApiDelegate::GetPlatformInfo(
    api::runtime::PlatformInfo* info) {
  info->os = api::runtime::PlatformOs::kAndroid;
  return true;
}

bool DesktopAndroidRuntimeApiDelegate::RestartDevice(
    std::string* error_message) {
  // TODO(crbug.com/373434594): Support device restart.
  NOTIMPLEMENTED();
  *error_message = "not implemented";
  return false;
}

}  // namespace extensions
