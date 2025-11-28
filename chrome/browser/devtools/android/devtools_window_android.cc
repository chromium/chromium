// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "build/android_buildflags.h"
#include "chrome/browser/devtools/android/jni/DevToolsWindowAndroid_jni.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "third_party/jni_zero/jni_zero.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "chrome/browser/devtools/devtools_window.h"
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

static void JNI_DevToolsWindowAndroid_OpenDevTools(
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

static jboolean JNI_DevToolsWindowAndroid_IsDevToolsAllowedFor(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& java_profile,
    const jni_zero::JavaParamRef<jobject>& java_web_contents) {
#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
  Profile* profile = Profile::FromJavaObject(java_profile);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return DevToolsWindow::AllowDevToolsFor(profile, web_contents) ? JNI_TRUE
                                                                 : JNI_FALSE;
#else
  return JNI_FALSE;
#endif
}

DEFINE_JNI(DevToolsWindowAndroid)
