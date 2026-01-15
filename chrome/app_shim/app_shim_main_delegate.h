// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_MAIN_DELEGATE_H_
#define CHROME_APP_SHIM_APP_SHIM_MAIN_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/app/content_main_delegate.h"

namespace app_mode {
struct ChromeAppModeInfo;
}

class AppShimController;
// AppShimMainDelegate is the ContentMainDelegate for the app shim process.
class AppShimMainDelegate : public content::ContentMainDelegate {
 public:
  explicit AppShimMainDelegate(
      const app_mode::ChromeAppModeInfo* app_mode_info);
  ~AppShimMainDelegate() override;

  // content::ContentMainDelegate:
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  std::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  bool ShouldInitializePerfetto(InvokedIn invoked_in) override;
  bool ShouldReconfigurePartitionAlloc() override;
  bool ShouldLoadV8Snapshot(const std::string& process_type) override;
  content::ContentClient* CreateContentClient() override;

 private:
  void InitializeLocale();

  const raw_ptr<const app_mode::ChromeAppModeInfo> app_mode_info_;
  std::unique_ptr<AppShimController> app_shim_controller_;
  std::unique_ptr<content::ContentClient> content_client_;
};

#endif  // CHROME_APP_SHIM_APP_SHIM_MAIN_DELEGATE_H_
