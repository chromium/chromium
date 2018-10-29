// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_process/service_process_control.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/task/post_task.h"
#include "chrome/common/service_process_util_posix.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/google_toolbox_for_mac/src/Foundation/GTMServiceManagement.h"

using content::BrowserThread;

void ServiceProcessControl::Launcher::DoRun() {
  base::ScopedCFTypeRef<CFDictionaryRef> launchd_plist(
      CreateServiceProcessLaunchdPlist(cmd_line_.get(), false));
  CFErrorRef error = NULL;
  if (!GTMSMJobSubmit(launchd_plist, &error)) {
    LOG(ERROR) << error;
    CFRelease(error);
  } else {
    launched_ = true;
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::Bind(&Launcher::Notify, this));
}
