// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define CHROME_APP_CHROME_MAIN_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/app/content_main_delegate.h"

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "chrome/browser/startup_data.h"
#endif

namespace base {
class CommandLine;
}

namespace tracing {
class TracingSamplerProfiler;
}

class ChromeContentBrowserClient;
class HeapProfilerController;

// Chrome implementation of ContentMainDelegate.
class ChromeMainDelegate : public content::ContentMainDelegate {
 public:
  static const char* const kNonWildcardDomainNonPortSchemes[];
  static const size_t kNonWildcardDomainNonPortSchemesSize;

  ChromeMainDelegate();

  // |exe_entry_point_ticks| is the time at which the main function of the
  // executable was entered, or null if not available.
  explicit ChromeMainDelegate(base::TimeTicks exe_entry_point_ticks);
  ~ChromeMainDelegate() override;

 protected:
  // content::ContentMainDelegate:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  void SandboxInitialized(const std::string& process_type) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if defined(OS_MACOSX)
  bool ProcessRegistersWithSystemProcess(
      const std::string& process_type) override;
  bool DelaySandboxInitialization(const std::string& process_type) override;
#elif defined(OS_LINUX)
  void ZygoteStarting(
      std::vector<std::unique_ptr<service_manager::ZygoteForkDelegate>>*
          delegates) override;
  void ZygoteForked() override;
#endif
  service_manager::ProcessType OverrideProcessType() override;
  void PreCreateMainMessageLoop() override;
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  void PostEarlyInitialization(bool is_running_tests) override;
  bool ShouldCreateFeatureList() override;
#endif  // !defined(CHROME_MULTIPLE_DLL_CHILD)
  void PostFieldTrialInitialization() override;

  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

#if defined(OS_MACOSX)
  void InitMacCrashReporter(const base::CommandLine& command_line,
                            const std::string& process_type);
  void SetUpInstallerPreferences(const base::CommandLine& command_line);
#endif  // defined(OS_MACOSX)

  ChromeContentClient chrome_content_client_;

  std::unique_ptr<ChromeContentBrowserClient> chrome_content_browser_client_;

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  std::unique_ptr<StartupData> startup_data_;
#endif

  std::unique_ptr<tracing::TracingSamplerProfiler> tracing_sampler_profiler_;

  // The controller schedules UMA heap profiles collections and forwarding down
  // the reporting pipeline.
  std::unique_ptr<HeapProfilerController> heap_profiler_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegate);
};

#endif  // CHROME_APP_CHROME_MAIN_DELEGATE_H_
