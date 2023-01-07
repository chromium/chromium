// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTDOWN_SIGNAL_HANDLERS_POSIX_H_
#define CHROME_BROWSER_SHUTDOWN_SIGNAL_HANDLERS_POSIX_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

// Runs a background thread that installs signal handlers to watch for shutdown
// signals like SIGTERM, SIGINT and SIGTERM. |shutdown_callback| is invoked on
// |task_runner| which is usually the main thread's task runner.
void InstallShutdownSignalHandlers(
    base::OnceCallback<void(int)> shutdown_callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

#endif  // CHROME_BROWSER_SHUTDOWN_SIGNAL_HANDLERS_POSIX_H_
