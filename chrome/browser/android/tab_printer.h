// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_PRINTER_H_
#define CHROME_BROWSER_ANDROID_TAB_PRINTER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace printing {

base::android::ScopedJavaLocalRef<jobject> GetPrintableForTab(
    const base::android::ScopedJavaLocalRef<jobject>& java_tab);

}  // namespace printing

#endif  // CHROME_BROWSER_ANDROID_TAB_PRINTER_H_
