// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_TEST_BROWSER_MANAGEMENT_CONTEXT_MIXIN_BROWSER_H_
#define CHROME_BROWSER_ENTERPRISE_TEST_BROWSER_MANAGEMENT_CONTEXT_MIXIN_BROWSER_H_

#include <memory>

#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"

namespace policy {
class FakeBrowserDMTokenStorage;
}  // namespace policy

namespace enterprise::test {

class ManagementContextMixinBrowser : public ManagementContextMixin {
 public:
  ManagementContextMixinBrowser(InProcessBrowserTestMixinHost* host,
                                PlatformBrowserTest* test_base,
                                ManagementContext management_context);

  ManagementContextMixinBrowser(const ManagementContextMixinBrowser&) = delete;
  ManagementContextMixinBrowser& operator=(
      const ManagementContextMixinBrowser&) = delete;

  ~ManagementContextMixinBrowser() override;

  // ManagementContextMixin:
  void ManageCloudUser() override;

 protected:
  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;
#endif

  // ManagementContextMixin:
  void SetCloudMachinePolicies(
      base::flat_map<std::string, std::optional<base::Value>> policy_entries)
      override;

 private:
  policy::FakeBrowserDMTokenStorage browser_dm_token_storage_;
};

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_TEST_BROWSER_MANAGEMENT_CONTEXT_MIXIN_BROWSER_H_
