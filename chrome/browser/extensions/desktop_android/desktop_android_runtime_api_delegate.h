// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_RUNTIME_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_RUNTIME_API_DELEGATE_H_

#include "extensions/browser/api/runtime/runtime_api_delegate.h"

namespace extensions {

// An extensions runtime API delegate for the desktop Android platform.
class DesktopAndroidRuntimeApiDelegate : public RuntimeAPIDelegate {
 public:
  DesktopAndroidRuntimeApiDelegate();
  DesktopAndroidRuntimeApiDelegate(const DesktopAndroidRuntimeApiDelegate&) =
      delete;
  DesktopAndroidRuntimeApiDelegate& operator=(
      const DesktopAndroidRuntimeApiDelegate&) = delete;
  ~DesktopAndroidRuntimeApiDelegate() override;

  // RuntimeAPIDelegate:
  void AddUpdateObserver(UpdateObserver* observer) override;
  void RemoveUpdateObserver(UpdateObserver* observer) override;
  void ReloadExtension(const ExtensionId& extension_id) override;
  bool CheckForUpdates(const ExtensionId& extension_id,
                       UpdateCheckCallback callback) override;
  void OpenURL(const GURL& uninstall_url) override;
  bool GetPlatformInfo(api::runtime::PlatformInfo* info) override;
  bool RestartDevice(std::string* error_message) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_RUNTIME_API_DELEGATE_H_
