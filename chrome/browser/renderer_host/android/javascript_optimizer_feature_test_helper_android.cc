// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/renderer_host/android/jni_headers/JavascriptOptimizerFeatureTestHelperAndroid_jni.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

static jboolean
JNI_JavascriptOptimizerFeatureTestHelperAndroid_AreJavascriptOptimizersEnabledOnWebContents(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  return !web_contents->GetPrimaryMainFrame()
              ->GetProcess()
              ->AreV8OptimizationsDisabled();
}

DEFINE_JNI(JavascriptOptimizerFeatureTestHelperAndroid)
