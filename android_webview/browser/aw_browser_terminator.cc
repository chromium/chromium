// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_terminator.h"

#include <unistd.h>
#include <memory>

#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/browser_jni_headers/AwBrowserProcess_jni.h"
#include "android_webview/common/aw_descriptors.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"

using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void GetJavaWebContentsForRenderProcess(
    content::RenderProcessHost* rph,
    std::vector<ScopedJavaGlobalRef<jobject>>* java_web_contents) {
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderViewHost* view = content::RenderViewHost::From(widget);
    if (view && rph == view->GetProcess()) {
      content::WebContents* wc = content::WebContents::FromRenderViewHost(view);
      if (wc) {
        java_web_contents->push_back(static_cast<ScopedJavaGlobalRef<jobject>>(
            wc->GetJavaWebContents()));
      }
    }
  }
}

void OnRenderProcessGone(
    const std::vector<ScopedJavaGlobalRef<jobject>>& java_web_contents,
    base::ProcessId child_process_pid,
    bool crashed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& java_wc : java_web_contents) {
    content::WebContents* wc =
        content::WebContents::FromJavaWebContents(java_wc);
    if (!wc)
      continue;

    AwRenderProcessGoneDelegate* delegate =
        AwRenderProcessGoneDelegate::FromWebContents(wc);
    if (!delegate)
      continue;

    switch (delegate->OnRenderProcessGone(child_process_pid, crashed)) {
      case AwRenderProcessGoneDelegate::RenderProcessGoneResult::kException:
        // Let the exception propagate back to the message loop.
        base::MessageLoopCurrentForUI::Get()->Abort();
        return;
      case AwRenderProcessGoneDelegate::RenderProcessGoneResult::kUnhandled:
        if (crashed) {
          // Keeps this log unchanged, CTS test uses it to detect crash.
          std::string message = base::StringPrintf(
              "Render process (%d)'s crash wasn't handled by all associated  "
              "webviews, triggering application crash.",
              child_process_pid);
          crash_reporter::CrashWithoutDumping(message);
        } else {
          // The render process was most likely killed for OOM or switching
          // WebView provider, to make WebView backward compatible, kills the
          // browser process instead of triggering crash.
          LOG(ERROR) << "Render process (" << child_process_pid << ") kill (OOM"
                     << " or update) wasn't handed by all associated webviews,"
                     << " killing application.";
          kill(getpid(), SIGKILL);
        }
        NOTREACHED();
        break;
      case AwRenderProcessGoneDelegate::RenderProcessGoneResult::kHandled:
        break;
    }
  }

  // By this point we have moved the minidump to the crash directory, so it can
  // now be copied and uploaded.
  Java_AwBrowserProcess_triggerMinidumpUploading(
      base::android::AttachCurrentThread());
}

}  // namespace

AwBrowserTerminator::AwBrowserTerminator() = default;

AwBrowserTerminator::~AwBrowserTerminator() = default;

void AwBrowserTerminator::OnChildExit(
    const crash_reporter::ChildExitObserver::TerminationInfo& info) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(info.process_host_id);

  crash_reporter::CrashMetricsReporter::GetInstance()->ChildProcessExited(info);

  if (info.normal_termination) {
    return;
  }

  LOG(ERROR) << "Renderer process (" << info.pid << ") crash detected (code "
             << info.crash_signo << ").";

  std::vector<ScopedJavaGlobalRef<jobject>> java_web_contents;
  GetJavaWebContentsForRenderProcess(rph, &java_web_contents);

  base::PostTask(FROM_HERE,
                 {content::BrowserThread::UI, base::TaskPriority::HIGHEST},
                 base::BindOnce(OnRenderProcessGone, java_web_contents,
                                info.pid, info.is_crashed()));
}

}  // namespace android_webview
