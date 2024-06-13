// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_executor.h"
#include "content/public/browser/browser_main_parts.h"

namespace crash_reporter {
class ChildExitObserver;
}

namespace metrics {
class MemoryMetricsLogger;
}

namespace android_webview {

class AwBrowserProcess;
class AwContentBrowserClient;

// Lifetime: Singleton
class AwBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit AwBrowserMainParts(AwContentBrowserClient* browser_client);

  AwBrowserMainParts(const AwBrowserMainParts&) = delete;
  AwBrowserMainParts& operator=(const AwBrowserMainParts&) = delete;

  ~AwBrowserMainParts() override;

  // Overriding methods from content::BrowserMainParts.
  int PreEarlyInitialization() override;
  int PreCreateThreads() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostCreateThreads() override;

 private:
  void RegisterSyntheticTrials();

  // Android specific UI SingleThreadTaskExecutor.
  std::unique_ptr<base::SingleThreadTaskExecutor> main_task_executor_;

  raw_ptr<AwContentBrowserClient> browser_client_;

  std::unique_ptr<metrics::MemoryMetricsLogger> metrics_logger_;

  std::unique_ptr<content::SyntheticTrialSyncer> synthetic_trial_syncer_;

  std::unique_ptr<AwBrowserProcess> browser_process_;
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
