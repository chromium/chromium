// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printing_init.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebContentsPrinter_jni.h"

using base::android::ScopedJavaLocalRef;

namespace printing {

namespace {

content::RenderFrameHost* GetTargetFrame(content::WebContents* web_contents,
                                         int32_t render_process_id,
                                         int32_t render_frame_id,
                                         bool print_selection_only) {
  if (render_process_id >= 0 && render_frame_id >= 0) {
    // A frame was named, so this job targets that specific document. Falling
    // back to another frame would print content the user did not request. The
    // frame may legitimately be gone or belong to a different WebContents if
    // the Tab swapped WebContents while a job was outstanding.
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (!rfh ||
        content::WebContents::FromRenderFrameHost(rfh) != web_contents) {
      return nullptr;
    }
    return rfh->IsActive() && rfh->IsRenderFrameLive() ? rfh : nullptr;
  }

  if (print_selection_only) {
    return nullptr;
  }

  content::RenderFrameHost* rfh = nullptr;
  if (base::FeatureList::IsEnabled(
          chrome::android::kPrintFallbackToPrimaryMainFrame)) {
    rfh = web_contents->GetPrimaryMainFrame();
  } else {
    rfh = GetFrameToPrint(web_contents);
  }
  if (!rfh || !rfh->IsActive() || !rfh->IsRenderFrameLive()) {
    return nullptr;
  }
  return rfh;
}

}  // namespace

static int64_t JNI_WebContentsPrinter_InitiatePrint(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    int32_t render_process_id,
    int32_t render_frame_id,
    bool print_selection_only) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return -1;
  }

  content::RenderFrameHost* rfh = GetTargetFrame(
      web_contents, render_process_id, render_frame_id, print_selection_only);
  if (!rfh) {
    return -1;
  }

  PrintViewManagerBasic* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents);
  if (!print_view_manager) {
    InitializePrintingForWebContents(web_contents);
    print_view_manager = PrintViewManagerBasic::FromWebContents(web_contents);
  }
  if (!print_view_manager || !print_view_manager->InitiatePrint(rfh)) {
    return -1;
  }

  content::GlobalRenderFrameHostId global_id = rfh->GetGlobalId();
  return (static_cast<int64_t>(global_id.child_id.GetUnsafeValue()) << 32) |
         (static_cast<uint64_t>(global_id.frame_routing_id) & 0xFFFFFFFFULL);
}

static bool JNI_WebContentsPrinter_Print(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    int32_t render_process_id,
    int32_t render_frame_id,
    bool print_selection_only) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return false;
  }

  content::RenderFrameHost* rfh = GetTargetFrame(
      web_contents, render_process_id, render_frame_id, print_selection_only);
  if (!rfh) {
    return false;
  }

  PrintViewManagerBasic* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents);
  if (!print_view_manager) {
    InitializePrintingForWebContents(web_contents);
    print_view_manager = PrintViewManagerBasic::FromWebContents(web_contents);
  }
  return print_view_manager &&
         print_view_manager->PrintNow(rfh, print_selection_only);
}

static void JNI_WebContentsPrinter_FinishPrint(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    int32_t render_process_id,
    int32_t render_frame_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return;
  }

  // Teardown must reach the frame the job was started for even if it is no
  // longer active (e.g. moved into BackForwardCache); otherwise the renderer
  // retains `print_in_progress_` permanently. Never substitute the current
  // main frame of a newly navigated page.
  const bool has_target_frame = render_process_id >= 0 && render_frame_id >= 0;
  content::RenderFrameHost* rfh =
      has_target_frame
          ? content::RenderFrameHost::FromID(render_process_id, render_frame_id)
          : nullptr;
  if (rfh && content::WebContents::FromRenderFrameHost(rfh) != web_contents) {
    rfh = nullptr;
  }

  PrintViewManagerBasic* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents);
  if (print_view_manager) {
    print_view_manager->FinishPrint(rfh);
  }
}

}  // namespace printing

DEFINE_JNI(WebContentsPrinter)
