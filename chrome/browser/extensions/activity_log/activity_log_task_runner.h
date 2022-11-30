// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_TASK_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_TASK_RUNNER_H_

#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace extensions {

// TODO(devlin): Similar to below, it's possible that SQL doesn't like running
// on multiple threads. We *might* be able to change this to a
// SequencedTaskRunner, but more investigation is needed.
const scoped_refptr<base::SingleThreadTaskRunner> GetActivityLogTaskRunner();

// TODO(devlin): It would be great to remove this, but we can't create a valid
// SQL database in unittests using the normal ActivityLogTaskRunner. Might be
// related to https://crbug.com/739945.
void SetActivityLogTaskRunnerForTesting(
    base::SingleThreadTaskRunner* task_runner);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_TASK_RUNNER_H_
