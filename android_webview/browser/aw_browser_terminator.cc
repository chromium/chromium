// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_terminator.h"

#include <unistd.h>

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_render_process.h"
#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_features.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"

using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

constexpr char kRenderProcessGoneHistogramName[] =
    "Android.WebView.OnRenderProcessGoneResult";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RenderProcessGoneResult {
  kJavaException = 0,
  kCrashNotHandled = 1,
  kKillNotHandled = 2,
  // kAllWebViewsHandled = 3, // Deprecated: use kCrashHandled/kKillHandled
  kCrashHandled = 4,
  kKillHandled = 5,
  kMaxValue = kKillHandled,
};

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
        base::UmaHistogramEnumeration(kRenderProcessGoneHistogramName,
                                      RenderProcessGoneResult::kJavaException);
        // Let the exception propagate back to the message loop.
        base::CurrentUIThread::Get()->Abort();
        return;
      case AwRenderProcessGoneDelegate::RenderProcessGoneResult::kUnhandled:
        if (crashed) {
          base::UmaHistogramEnumeration(
              kRenderProcessGoneHistogramName,
              RenderProcessGoneResult::kCrashNotHandled);
          // Keeps this log unchanged, CTS test uses it to detect crash.
          std::string message = base::StringPrintf(
              "Render process (%d)'s crash wasn't handled by all associated  "
              "webviews, triggering application crash.",
              child_process_pid);
          crash_reporter::CrashWithoutDumping(message);
        } else {
          base::UmaHistogramEnumeration(
              kRenderProcessGoneHistogramName,
              RenderProcessGoneResult::kKillNotHandled);
          // The render process was most likely killed for OOM or switching
          // WebView provider, to make WebView backward compatible, kills the
          // browser process instead of triggering crash.
          LOG(ERROR) << "Render process (" << child_process_pid << ") kill (OOM"
                     << " or update) wasn't handed by all associated webviews,"
                     << " killing application.";
          kill(getpid(), SIGKILL);
        }
        NOTREACHED();
      case AwRenderProcessGoneDelegate::RenderProcessGoneResult::kHandled:
        // Don't log UMA yet. This WebView may be handled, but we need to wait
        // until we're out of the loop to know if all WebViews were handled.
        break;
    }
  }
  // If we reached this point, it means the crash was handled for all WebViews.
  if (crashed) {
    base::UmaHistogramEnumeration(kRenderProcessGoneHistogramName,
                                  RenderProcessGoneResult::kCrashHandled);
  } else {
    base::UmaHistogramEnumeration(kRenderProcessGoneHistogramName,
                                  RenderProcessGoneResult::kKillHandled);
  }

  // By this point we have moved the minidump to the crash directory, so it can
  // now be copied and uploaded.
  AwBrowserProcess::TriggerMinidumpUploading();
}

}  // namespace

AwBrowserTerminator::AwBrowserTerminator() = default;

AwBrowserTerminator::~AwBrowserTerminator() = default;

void AwBrowserTerminator::OnChildExit(
    const crash_reporter::ChildExitObserver::TerminationInfo& info) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(info.process_host_id);

  crash_reporter::CrashMetricsReporter::GetInstance()->ChildProcessExited(info);

  // If the process has never been used, this is the spare render process.
  // Treat this as if it never existed since it's an internal performance
  // optimization.
  if (base::FeatureList::IsEnabled(
          features::kCreateSpareRendererOnBrowserContextCreation) &&
      rph && AwRenderProcess::IsUnused(rph)) {
    return;
  }

  if (info.normal_termination) {
    return;
  }

  LOG(ERROR) << "Renderer process (" << info.pid << ") crash detected (code "
             << info.crash_signo << ").";

  std::vector<ScopedJavaGlobalRef<jobject>> java_web_contents;
  GetJavaWebContentsForRenderProcess(rph, &java_web_contents);

  content::GetUIThreadTaskRunner({base::TaskPriority::HIGHEST})
      ->PostTask(FROM_HERE,
                 base::BindOnce(OnRenderProcessGone, java_web_contents,
                                info.pid, info.is_crashed()));
}

}  // namespace android_webview
