// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_AW_MAIN_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_AW_MAIN_DELEGATE_H_

#include <memory>

#include "android_webview/common/aw_content_client.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class BrowserMainRunner;
}

namespace safe_browsing {
class SafeBrowsingApiHandler;
}

namespace android_webview {

class AwContentBrowserClient;
class AwContentGpuClient;
class AwContentRendererClient;
class AwContentUtilityClient;

// Android WebView implementation of ContentMainDelegate. The methods in
// this class runs per process, (browser and renderer) so when making changes
// make sure to properly conditionalize for browser vs. renderer wherever
// needed.
class AwMainDelegate : public content::ContentMainDelegate {
 public:
  AwMainDelegate();
  ~AwMainDelegate() override;

 private:
  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
  bool ShouldCreateFeatureList() override;
  void PostEarlyInitialization(bool is_running_tests) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  AwContentClient content_client_;
  std::unique_ptr<AwContentBrowserClient> content_browser_client_;
  std::unique_ptr<AwContentGpuClient> content_gpu_client_;
  std::unique_ptr<AwContentRendererClient> content_renderer_client_;
  std::unique_ptr<AwContentUtilityClient> content_utility_client_;
  std::unique_ptr<safe_browsing::SafeBrowsingApiHandler>
      safe_browsing_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(AwMainDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_AW_MAIN_DELEGATE_H_
