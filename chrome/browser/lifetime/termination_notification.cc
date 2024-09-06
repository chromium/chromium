// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/termination_notification.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"

namespace browser_shutdown {
namespace {

base::OnceClosureList& GetAppTerminatingCallbackList() {
  static base::NoDestructor<base::OnceClosureList> callback_list;
  return *callback_list;
}

}  // namespace

base::CallbackListSubscription AddAppTerminatingCallback(
    base::OnceClosure app_terminating_callback) {
  return GetAppTerminatingCallbackList().Add(
      std::move(app_terminating_callback));
}

void NotifyAppTerminating() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static bool notified = false;
  if (notified) {
    return;
  }
  notified = true;
  GetAppTerminatingCallbackList().Notify();
}

}  // namespace browser_shutdown
