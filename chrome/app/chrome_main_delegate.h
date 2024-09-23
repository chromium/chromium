// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define CHROME_APP_CHROME_MAIN_DELEGATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/startup_timestamps.h"
#include "chrome/browser/startup_data.h"
#include "chrome/common/chrome_content_client.h"
#include "components/memory_system/memory_system.h"
#include "content/public/app/content_main_delegate.h"

namespace base {
class CommandLine;
}

namespace chromeos {
class LacrosService;
}

namespace tracing {
class TracingSamplerProfiler;
}

class ChromeContentBrowserClient;
class ChromeContentUtilityClient;
class MainThreadStackSamplingProfiler;

// Chrome implementation of ContentMainDelegate.
class ChromeMainDelegate : public content::ContentMainDelegate {
 public:
  static const char* const kNonWildcardDomainNonPortSchemes[];
  static const size_t kNonWildcardDomainNonPortSchemesSize;

#if BUILDFLAG(IS_ANDROID)
  ChromeMainDelegate();
#endif

  // `timestamps.exe_entry_point_ticks` is the time at which the main function
  // of the executable was entered. On Windows, StartupTimestamps contains
  // timing information for calls to base::PreReadFile. `timestamps`' lifetime
  // does not need to last beyond the constructor call.
  explicit ChromeMainDelegate(const StartupTimestamps& timestamps);

  ChromeMainDelegate(const ChromeMainDelegate&) = delete;
  ChromeMainDelegate& operator=(const ChromeMainDelegate&) = delete;

  ~ChromeMainDelegate() override;

 protected:
  // content::ContentMainDelegate:
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  void SandboxInitialized(const std::string& process_type) override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void ZygoteStarting(std::vector<std::unique_ptr<content::ZygoteForkDelegate>>*
                          delegates) override;
  void ZygoteForked() override;
#endif
  std::optional<int> PreBrowserMain() override;
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  void CreateThreadPool(std::string_view name) override;
#if BUILDFLAG(IS_WIN)
  bool ShouldHandleConsoleControlEvents() override;
#endif

  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

  // Initialization that happens in all process types.
  void CommonEarlyInitialization(InvokedIn invoked_in);

  // Initializes |tracing_sampler_profiler_|. Deletes any existing
  // |tracing_sampler_profiler_| as well.
  void SetupTracing();

#if BUILDFLAG(IS_MAC)
  void InitMacCrashReporter(const base::CommandLine& command_line,
                            const std::string& process_type);
  void SetUpInstallerPreferences(const base::CommandLine& command_line);
#endif  // BUILDFLAG(IS_MAC)

  void InitializeMemorySystem();

  std::unique_ptr<ChromeContentBrowserClient> chrome_content_browser_client_;
  std::unique_ptr<ChromeContentUtilityClient> chrome_content_utility_client_;
  std::unique_ptr<tracing::TracingSamplerProfiler> tracing_sampler_profiler_;

  ChromeContentClient chrome_content_client_;

  memory_system::MemorySystem memory_system_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<chromeos::LacrosService> lacros_service_;
#endif

#if !BUILDFLAG(IS_ANDROID)
  // The sampling profiler exists until the `ChromeContentBrowserClient` is
  // created and ownership is passed to it.
  std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler_;
#endif
};

#endif  // CHROME_APP_CHROME_MAIN_DELEGATE_H_
