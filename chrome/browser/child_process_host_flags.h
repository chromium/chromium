// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_PROCESS_HOST_FLAGS_H_
#define CHROME_BROWSER_CHILD_PROCESS_HOST_FLAGS_H_

#include "build/build_config.h"
#include "content/public/browser/child_process_host.h"

namespace chrome {

// Flags for Chrome specific child processes to resolve the appropriate process
// via ChromeContentBrowserClient::GetChildProcessSuffix().
enum ChildProcessHostFlags {
#if BUILDFLAG(IS_MAC)
  // Starts a child process with the macOS alert style to show notifications as
  // alerts instead of banners which are shown by the main app.
  kChildProcessHelperAlerts =
      content::ChildProcessHost::CHILD_EMBEDDER_FIRST + 1,
#endif  // BUILDFLAG(IS_MAC)
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CHILD_PROCESS_HOST_FLAGS_H_
