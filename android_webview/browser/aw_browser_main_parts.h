// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/task/single_thread_task_executor.h"
#include "content/public/browser/browser_main_parts.h"

namespace android_webview {

class AwBrowserProcess;
class AwContentBrowserClient;
class MemoryMetricsLogger;

class AwBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit AwBrowserMainParts(AwContentBrowserClient* browser_client);
  ~AwBrowserMainParts() override;

  // Overriding methods from content::BrowserMainParts.
  int PreEarlyInitialization() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostCreateThreads() override;

 private:
  // Android specific UI SingleThreadTaskExecutor.
  std::unique_ptr<base::SingleThreadTaskExecutor> main_task_executor_;

  AwContentBrowserClient* browser_client_;

  std::unique_ptr<MemoryMetricsLogger> metrics_logger_;

  std::unique_ptr<AwBrowserProcess> browser_process_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserMainParts);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
