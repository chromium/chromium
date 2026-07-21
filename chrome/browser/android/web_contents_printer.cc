// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebContentsPrinter_jni.h"

using base::android::ScopedJavaLocalRef;

namespace printing {

static bool JNI_WebContentsPrinter_Print(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    int32_t render_process_id,
    int32_t render_frame_id,
    bool print_selection_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return false;
  }

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (rfh) {
    DCHECK_EQ(content::WebContents::FromRenderFrameHost(rfh), web_contents);
  }
  // If the target frame is invalid, inactive, or no longer live:
  // - For selection printing, fail safely to avoid printing the whole
  //   page/wrong frame.
  // - For normal printing, fall back to the default main frame.
  if (!rfh || !rfh->IsActive() || !rfh->IsRenderFrameLive()) {
    if (print_selection_only) {
      return false;
    }
    rfh = GetFrameToPrint(web_contents);
  }

  if (!rfh || !rfh->IsActive() || !rfh->IsRenderFrameLive()) {
    return false;
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);

  PrintViewManagerBasic* print_view_manager =
      PrintViewManagerBasic::FromWebContents(contents);
  return print_view_manager &&
         print_view_manager->PrintNow(rfh, print_selection_only);
}

}  // namespace printing

DEFINE_JNI(WebContentsPrinter)
