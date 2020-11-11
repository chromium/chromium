// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace enterprise_reporting {

class ChromeBrowserExtraSetUp : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserExtraSetUp(
      policy::ChromeBrowserCloudManagementController::Observer* observer)
      : observer_(observer) {}
  void PreMainMessageLoopStart() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->AddObserver(observer_);
  }

 private:
  policy::ChromeBrowserCloudManagementController::Observer* observer_;
};

class ReportSchedulerTest
    : public InProcessBrowserTest,
      public policy::ChromeBrowserCloudManagementController::Observer {
 public:
  ReportSchedulerTest() {
    policy::BrowserDMTokenStorage::SetForTesting(&storage_);
  }
  ~ReportSchedulerTest() override = default;

  void SetUpOnMainThread() override {
    g_browser_process->local_state()->SetBoolean(kCloudReportingEnabled, true);
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<ChromeBrowserExtraSetUp>(this));
  }

  void TearDownOnMainThread() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->RemoveObserver(this);
  }

  // policy::ChromeBrowserCloudManagementController::Observer
  void OnCloudReportingLaunched() override {
    has_cloud_reporting_launched = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForCloudReportingLaunching() {
    if (has_cloud_reporting_launched)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif
 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::FakeBrowserDMTokenStorage storage_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool has_cloud_reporting_launched = false;
};

// Verify that the cloud reporting can be launched and shutdown with the browser
// without crash.
IN_PROC_BROWSER_TEST_F(ReportSchedulerTest, LaunchTest) {
  WaitForCloudReportingLaunching();
}

}  // namespace enterprise_reporting
