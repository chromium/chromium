// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_printer.h"

#include "chrome/android/chrome_jni_headers/TabPrinter_jni.h"
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

using base::android::ScopedJavaLocalRef;

namespace printing {

ScopedJavaLocalRef<jobject> GetPrintableForTab(
    const ScopedJavaLocalRef<jobject>& java_tab) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabPrinter_getPrintable(env, java_tab);
}

static jboolean JNI_TabPrinter_Print(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jint render_process_id,
    jint render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return false;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!rfh)
    rfh = GetFrameToPrint(web_contents);

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);

  PrintViewManagerBasic* print_view_manager =
      PrintViewManagerBasic::FromWebContents(contents);
  return print_view_manager && print_view_manager->PrintNow(rfh);
}

}  // namespace printing
