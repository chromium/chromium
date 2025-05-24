// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "build/android_buildflags.h"
#include "chrome/browser/devtools/android/jni/DevToolsWindowAndroid_jni.h"
#include "content/public/common/buildflags.h"
#include "third_party/jni_zero/jni_zero.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "chrome/browser/devtools/devtools_window.h"
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

void JNI_DevToolsWindowAndroid_OpenDevTools(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& java_web_contents) {
#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  DevToolsWindow::OpenDevToolsWindow(
      web_contents, DevToolsToggleAction::Show(),
      DevToolsOpenedByAction::kMainMenuOrMainShortcut);
#else
  NOTREACHED();
#endif
}
