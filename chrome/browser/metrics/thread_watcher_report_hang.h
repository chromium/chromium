// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_THREAD_WATCHER_REPORT_HANG_H_
#define CHROME_BROWSER_METRICS_THREAD_WATCHER_REPORT_HANG_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

namespace metrics {

#if !BUILDFLAG(IS_ANDROID)

// This function makes it possible to tell from the callstack why shutdown is
// taking too long.
NOINLINE void ShutdownHang();

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_THREAD_WATCHER_REPORT_HANG_H_
