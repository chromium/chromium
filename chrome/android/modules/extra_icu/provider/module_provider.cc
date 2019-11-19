// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/extra_icu/provider/module_provider.h"
#include "chrome/android/modules/extra_icu/provider/jni_headers/ExtraIcuModuleProvider_jni.h"

namespace extra_icu {

// static
bool ModuleProvider::IsModuleInstalled() {
  return Java_ExtraIcuModuleProvider_isModuleInstalled(
      base::android::AttachCurrentThread());
}

}  // namespace extra_icu
