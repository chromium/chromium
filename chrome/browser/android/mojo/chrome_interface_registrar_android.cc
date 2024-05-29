// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/mojo/chrome_interface_registrar_android.h"

#include <jni.h>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeInterfaceRegistrar_jni.h"

void RegisterChromeJavaMojoInterfaces() {
  Java_ChromeInterfaceRegistrar_registerMojoInterfaces(
      base::android::AttachCurrentThread());
}
