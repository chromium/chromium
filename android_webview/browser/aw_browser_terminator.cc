// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_terminator.h"

#include <unistd.h>
#include <memory>

#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/sync_socket.h"
#include "base/task/post_task.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"
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
#include "jni/AwBrowserProcess_jni.h"

using content::BrowserThread;

namespace android_webview {

namespace {

void GetAwRenderProcessGoneDelegatesForRenderProcess(
    int render_process_id,
    std::vector<AwRenderProcessGoneDelegate*>* delegates) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderViewHost* view = content::RenderViewHost::From(widget);
    if (view && rph == view->GetProcess()) {
      content::WebContents* wc = content::WebContents::FromRenderViewHost(view);
      if (wc) {
        AwRenderProcessGoneDelegate* delegate =
            AwRenderProcessGoneDelegate::FromWebContents(wc);
        if (delegate)
          delegates->push_back(delegate);
      }
    }
  }
}

void OnRenderProcessGone(int process_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<AwRenderProcessGoneDelegate*> delegates;
  GetAwRenderProcessGoneDelegatesForRenderProcess(process_host_id, &delegates);
  for (auto* delegate : delegates)
    delegate->OnRenderProcessGone(process_host_id);
}

void OnRenderProcessGoneDetail(int process_host_id,
                               base::ProcessHandle child_process_pid,
                               bool crashed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<AwRenderProcessGoneDelegate*> delegates;
  GetAwRenderProcessGoneDelegatesForRenderProcess(process_host_id, &delegates);
  for (auto* delegate : delegates) {
    if (!delegate->OnRenderProcessGoneDetail(child_process_pid, crashed)) {
      if (crashed) {
        crash_reporter::SuppressDumpGeneration();
        // Keeps this log unchanged, CTS test uses it to detect crash.
        LOG(FATAL) << "Render process (" << child_process_pid << ")'s crash"
                   << " wasn't handled by all associated  webviews, triggering"
                   << " application crash.";
      } else {
        // The render process was most likely killed for OOM or switching
        // WebView provider, to make WebView backward compatible, kills the
        // browser process instead of triggering crash.
        LOG(ERROR) << "Render process (" << child_process_pid << ") kill (OOM"
                   << " or update) wasn't handed by all associated webviews,"
                   << " killing application.";
        kill(getpid(), SIGKILL);
      }
    }
  }

  // By this point we have moved the minidump to the crash directory, so it can
  // now be copied and uploaded.
  Java_AwBrowserProcess_triggerMinidumpUploading(
      base::android::AttachCurrentThread());
}

}  // namespace

AwBrowserTerminator::AwBrowserTerminator(base::FilePath crash_dump_dir)
    : crash_dump_dir_(crash_dump_dir) {}

AwBrowserTerminator::~AwBrowserTerminator() {}

void AwBrowserTerminator::OnChildStart(
    int process_host_id,
    content::PosixFileDescriptorInfo* mappings) {
  DCHECK(content::CurrentlyOnProcessLauncherTaskRunner());

  base::AutoLock auto_lock(process_host_id_to_pipe_lock_);
  DCHECK(!ContainsKey(process_host_id_to_pipe_, process_host_id));

  auto local_pipe = std::make_unique<base::SyncSocket>();
  auto child_pipe = std::make_unique<base::SyncSocket>();
  if (base::SyncSocket::CreatePair(local_pipe.get(), child_pipe.get())) {
    process_host_id_to_pipe_[process_host_id] = std::move(local_pipe);
    mappings->Transfer(kAndroidWebViewCrashSignalDescriptor,
                       base::ScopedFD(dup(child_pipe->handle())));
  }
  if (crash_reporter::IsCrashReporterEnabled()) {
    base::ScopedFD file(
        breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFileForChild(
            process_host_id));
    if (file != base::kInvalidPlatformFile)
      mappings->Transfer(kAndroidMinidumpDescriptor, std::move(file));
  }
}

void AwBrowserTerminator::OnChildExitAsync(
    const ::crash_reporter::ChildExitObserver::TerminationInfo& info,
    base::FilePath crash_dump_dir,
    std::unique_ptr<base::SyncSocket> pipe) {
  if (crash_reporter::IsCrashReporterEnabled()) {
    breakpad::CrashDumpManager::GetInstance()->ProcessMinidumpFileFromChild(
        crash_dump_dir, info);
  }

  if (!pipe.get() || info.normal_termination)
    return;

  bool crashed = false;

  // If the child process hasn't written anything into the pipe. This implies
  // that it was terminated via SIGKILL by the low memory killer.
  if (pipe->Peek() >= sizeof(int)) {
    int exit_code;
    pipe->Receive(&exit_code, sizeof(exit_code));
    LOG(ERROR) << "Renderer process (" << info.pid << ") crash detected (code "
               << exit_code << ").";
    crashed = true;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OnRenderProcessGoneDetail, info.process_host_id, info.pid,
                     crashed));
}

void AwBrowserTerminator::OnChildExit(
    const ::crash_reporter::ChildExitObserver::TerminationInfo& info) {
  std::unique_ptr<base::SyncSocket> pipe;

  {
    base::AutoLock auto_lock(process_host_id_to_pipe_lock_);
    // We might get a NOTIFICATION_RENDERER_PROCESS_TERMINATED and a
    // NOTIFICATION_RENDERER_PROCESS_CLOSED. In that case we only want
    // to process the first notification.
    const auto& iter = process_host_id_to_pipe_.find(info.process_host_id);
    if (iter != process_host_id_to_pipe_.end()) {
      pipe = std::move(iter->second);
      DCHECK(pipe->handle() != base::SyncSocket::kInvalidHandle);
      process_host_id_to_pipe_.erase(iter);
    }
  }
  if (pipe.get()) {
    OnRenderProcessGone(info.process_host_id);
  }

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&AwBrowserTerminator::OnChildExitAsync, info,
                     crash_dump_dir_, std::move(pipe)));
}

}  // namespace android_webview
