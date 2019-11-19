// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROVIDER_RUN_ON_UI_THREAD_BLOCKING_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_RUN_ON_UI_THREAD_BLOCKING_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Runs code synchronously on the UI thread. Should never be called directly
// from the UI thread. To be used only within the provider classes.
class RunOnUIThreadBlocking {
 public:
  // Runs the provided runnable in the UI thread synchronously.
  // The runnable argument can be defined using base::Bind.
  template <typename Signature>
  static void Run(base::Callback<Signature> runnable) {
    DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::WaitableEvent finished(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&RunOnUIThreadBlocking::RunOnUIThread<Signature>,
                       runnable, &finished));
    finished.Wait();
  }

 private:
  template <typename Signature>
  static void RunOnUIThread(base::Callback<Signature> runnable,
                            base::WaitableEvent* finished) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    runnable.Run();
    finished->Signal();
  }
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_RUN_ON_UI_THREAD_BLOCKING_H_
