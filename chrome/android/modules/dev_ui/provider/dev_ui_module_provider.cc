// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/dev_ui/provider/dev_ui_module_provider.h"

#include <utility>

#include "base/android/jni_android.h"
#include "chrome/android/modules/dev_ui/provider/dev_ui_install_listener.h"
#include "chrome/android/modules/dev_ui/provider/jni_headers/DevUiModuleProvider_jni.h"

namespace dev_ui {

namespace {

// Global DevUiModuleProvider instance for testing.
DevUiModuleProvider* g_test_instance = nullptr;

}  // namespace

// Destructor is public to enable management by std::unique_ptr<>.
DevUiModuleProvider::~DevUiModuleProvider() = default;

// static
DevUiModuleProvider* DevUiModuleProvider::GetInstance() {
  if (g_test_instance)
    return g_test_instance;

  static DevUiModuleProvider instance;
  return &instance;
}

// static
void DevUiModuleProvider::SetTestInstance(DevUiModuleProvider* test_instance) {
  g_test_instance = test_instance;
}

bool DevUiModuleProvider::ModuleInstalled() {
  return Java_DevUiModuleProvider_isModuleInstalled(
      base::android::AttachCurrentThread());
}

void DevUiModuleProvider::InstallModule(
    base::OnceCallback<void(bool)> on_complete) {
  auto* listener = DevUiInstallListener::Create(std::move(on_complete));
  // This should always return, since there is no InfoBar UI to retry (thus
  // avoiding crbug.com/996925 and crbug.com/996959).
  Java_DevUiModuleProvider_installModule(base::android::AttachCurrentThread(),
                                         listener->j_listener());
}

void DevUiModuleProvider::EnsureLoaded() {
  Java_DevUiModuleProvider_ensureNativeLoaded(
      base::android::AttachCurrentThread());
}

DevUiModuleProvider::DevUiModuleProvider() = default;

}  // namespace dev_ui
