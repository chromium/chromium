// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/result_codes.h"

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
#include "chrome/browser/downgrade/downgrade_manager.h"
#endif

class BrowserProcessImpl;
class ChromeBrowserMainExtraParts;
class StartupData;
class Profile;
class StartupBrowserCreator;
class ShutdownWatcherHelper;
class WebUsbDetector;

namespace base {
class CommandLine;
class RunLoop;
}  // namespace base

namespace content {
class SyntheticTrialSyncer;
}

namespace tracing {
class TraceEventSystemStatsMonitor;
}

class ChromeBrowserMainParts : public content::BrowserMainParts {
 public:
  ChromeBrowserMainParts(const ChromeBrowserMainParts&) = delete;
  ChromeBrowserMainParts& operator=(const ChromeBrowserMainParts&) = delete;
  ~ChromeBrowserMainParts() override;

  // Add additional ChromeBrowserMainExtraParts.
  void AddParts(std::unique_ptr<ChromeBrowserMainExtraParts> parts);

#if !BUILDFLAG(IS_ANDROID)
  // Returns the RunLoop that would be run by MainMessageLoopRun. This is used
  // by InProcessBrowserTests to allow them to run until the BrowserProcess is
  // ready for the browser to exit.
  static std::unique_ptr<base::RunLoop> TakeRunLoopForTest();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
  // Handles notifications from other processes. The function receives the
  // command line and directory with which the other Chrome process was
  // launched. Return true if the command line will be handled within the
  // current browser instance or false if the remote process should handle it
  // (i.e., because the current process is shutting down).
  static bool ProcessSingletonNotificationCallback(
      base::CommandLine command_line,
      const base::FilePath& current_directory);
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

 protected:
  ChromeBrowserMainParts(bool is_integration_test, StartupData* startup_data);

  // content::BrowserMainParts overrides.
  // These are called in-order by content::BrowserMainLoop.
  // Each stage calls the same stages in any ChromeBrowserMainExtraParts added
  // with AddParts() from ChromeContentBrowserClient::CreateBrowserMainParts.
  int PreEarlyInitialization() override;
  void PostEarlyInitialization() override;
  void ToolkitInitialized() override;
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  int PreMainMessageLoopRun() override;
#if !BUILDFLAG(IS_ANDROID)
  bool ShouldInterceptMainMessageLoopRun() override;
#endif
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void OnFirstIdle() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  // Additional stages for ChromeBrowserMainExtraParts. These stages are called
  // in order from PreMainMessageLoopRun(). See implementation for details.
  virtual void PreProfileInit();
  // `PostProfileInit()` is called for each regular profile that is created. The
  // first call has `is_initial_profile`=true, and subsequent calls have
  // `is_initial_profile`=false.
  // It may be called during startup if a profile is loaded immediately, or
  // later if the profile picker is shown.
  virtual void PostProfileInit(Profile* profile, bool is_initial_profile);
  virtual void PreBrowserStart();
  virtual void PostBrowserStart();

  // Displays a warning message that we can't find any locale data files.
  virtual void ShowMissingLocaleMessageBox() = 0;

  const base::FilePath& user_data_dir() const {
    return user_data_dir_;
  }

 protected:
  // Returns whether ChromeContentBrowserClient::CreateBrowserMainParts was
  // invoked as part of an integration (browser) test.
  // Avoid writing test-only conditions in product code if at all possible.
  bool is_integration_test() const { return is_integration_test_; }

 private:
  class ProfileInitManager;
  friend class ChromeBrowserMainPartsTestApi;

  // Constructs the metrics service and initializes metrics recording.
  void SetupMetrics();

  // Starts recording of metrics. This can only be called after we have a file
  // thread.
  static void StartMetricsRecording();

  // Record time from process startup to present time in an UMA histogram.
  void RecordBrowserStartupTime();

  // Calling during PreEarlyInitialization() to complete the remaining tasks
  // after the local state is loaded. Return value is an exit status,
  // RESULT_CODE_NORMAL_EXIT indicates success. If the return value is
  // RESULT_CODE_MISSING_DATA, then |failed_to_load_resource_bundle| indicates
  // if the ResourceBundle couldn't be loaded.
  int OnLocalStateLoaded(bool* failed_to_load_resource_bundle);

  // Applies any preferences (to local state) needed for first run. This is
  // always called and early outs if not first-run. Return value is an exit
  // status, RESULT_CODE_NORMAL_EXIT indicates success.
  int ApplyFirstRunPrefs();

  // Methods for Main Message Loop -------------------------------------------

  int PreCreateThreadsImpl();
  int PreMainMessageLoopRunImpl();

  // Wrapper for `PostProfileInit()` that provides to it the right
  // `is_initial_profile` value.
  void CallPostProfileInit(Profile* profile);

  // Members initialized on construction ---------------------------------------
  const bool is_integration_test_;
  const raw_ptr<StartupData> startup_data_;

  int result_code_ = content::RESULT_CODE_NORMAL_EXIT;

#if !BUILDFLAG(IS_ANDROID)
  // Create ShutdownWatcherHelper object for watching jank during shutdown.
  // Please keep |shutdown_watcher| as the first object constructed, and hence
  // it is destroyed last.
  std::unique_ptr<ShutdownWatcherHelper> shutdown_watcher_;

  std::unique_ptr<WebUsbDetector> web_usb_detector_;
#endif  // !BUILDFLAG(IS_ANDROID)

  // Vector of additional ChromeBrowserMainExtraParts.
  // Parts are deleted in the inverse order they are added.
  std::vector<std::unique_ptr<ChromeBrowserMainExtraParts>> chrome_extra_parts_;

  // The system stats monitor used by chrome://tracing. This doesn't do anything
  // until tracing of the |system_stats| category is enabled.
  std::unique_ptr<tracing::TraceEventSystemStatsMonitor>
      trace_event_system_stats_monitor_;

  std::unique_ptr<content::SyntheticTrialSyncer> synthetic_trial_syncer_;

  // ERP client instance, serving all reporting needs in the browser.
  reporting::ReportQueueProvider::SmartPtr<reporting::ReportingClient>
      reporting_client_{nullptr, base::OnTaskRunnerDeleter(nullptr)};

  // Members initialized after / released before main_message_loop_ ------------

  std::unique_ptr<BrowserProcessImpl> browser_process_;

#if !BUILDFLAG(IS_ANDROID)
  // Browser creation happens on the Java side in Android.
  std::unique_ptr<StartupBrowserCreator> browser_creator_;
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Members needed across shutdown methods.
  bool restart_last_session_ = false;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  downgrade::DowngradeManager downgrade_manager_;
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Android's first run is done in Java instead of native. Chrome OS does not
  // use master preferences.
  std::unique_ptr<first_run::MasterPrefs> master_prefs_;
#endif

  base::FilePath user_data_dir_;

  // Indicates that the initial profile has been created and we started
  // executing `PostProfileInit()` for it.
  bool initialized_initial_profile_ = false;

  // Observer that triggers `PostProfileInit()` when new user profiles are
  // created.
  // Must be deleted before `browser_process_`.
  std::unique_ptr<ProfileInitManager> profile_init_manager_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
