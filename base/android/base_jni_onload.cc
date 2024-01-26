// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/functional/bind.h"

namespace base {
namespace android {

bool OnJNIOnLoadInit() {
  InitAtExitManager();
  return true;
}

}  // namespace android
}  // namespace base
