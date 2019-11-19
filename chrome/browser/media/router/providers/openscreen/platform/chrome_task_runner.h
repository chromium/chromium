// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TASK_RUNNER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"

namespace media_router {

class ChromeTaskRunner final : public openscreen::platform::TaskRunner {
 public:
  explicit ChromeTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ChromeTaskRunner(const ChromeTaskRunner&) = delete;
  ChromeTaskRunner(ChromeTaskRunner&&) = delete;
  ChromeTaskRunner& operator=(const ChromeTaskRunner&) = delete;
  ChromeTaskRunner& operator=(ChromeTaskRunner&&) = delete;

  // TaskRunner overrides
  ~ChromeTaskRunner() final;
  void PostPackagedTask(openscreen::platform::TaskRunner::Task task) final;
  void PostPackagedTaskWithDelay(
      openscreen::platform::TaskRunner::Task task,
      openscreen::platform::Clock::duration delay) final;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TASK_RUNNER_H_
