// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_main_delegate.h"

#include "base/apple/bundle_locations.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/app_shim/app_shim_application.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "components/remote_cocoa/app_shim/application_bridge.h"
#include "components/variations/variations_crash_keys.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/system/functions.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

// Called when the app shim process receives a bad IPC message.
void HandleBadMessage(const std::string& error) {
  LOG(ERROR) << "Mojo error in app shim process: " << error;
  mojo::debug::ScopedMessageErrorCrashKey crash_key_value(error);
  base::debug::DumpWithoutCrashing();
}

}  // namespace

AppShimMainDelegate::AppShimMainDelegate(
    const app_mode::ChromeAppModeInfo* app_mode_info)
    : app_mode_info_(app_mode_info) {}

AppShimMainDelegate::~AppShimMainDelegate() = default;

std::optional<int> AppShimMainDelegate::BasicStartupComplete() {
  chrome::RegisterPathProvider();

  // Note that `info->user_data_dir` for shims contains the app data path,
  // <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/.
  const base::FilePath user_data_dir =
      base::FilePath(app_mode_info_->user_data_dir)
          .DirName()
          .DirName()
          .DirName();
  base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_USER_DATA, user_data_dir, /*is_absolute=*/false,
      /*create=*/false);

  remote_cocoa::ApplicationBridge::SetIsOutOfProcessAppShim();

  return std::nullopt;
}

void AppShimMainDelegate::PreSandboxStartup() {
  ChromeCrashReporterClient::Create();
  crash_reporter::InitializeCrashpad(true, "app_shim");

  // Initialize features and field trials, either from command line or from
  // file in user data dir.
  AppShimController::PreInitFeatureState(
      *base::CommandLine::ForCurrentProcess());

  InitializeLocale();

  // Mojo initialization. It's necessary to call Mojo's InitFeatures() to ensure
  // we're using the same IPC implementation as the browser.
  mojo::core::InitFeatures();
  // We're using an isolated Mojo connection between the browser and this
  // process, so this process must act as a broker.
  mojo::core::Configuration config;
  config.is_broker_process = true;
  mojo::core::Init(config);
  mojo::SetDefaultProcessErrorHandler(base::BindRepeating(&HandleBadMessage));
}

std::variant<int, content::MainFunctionParams> AppShimMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  if (process_type != switches::kAppShim) {
    return std::move(main_function_params);
  }

  // Launch the IO thread.
  base::Thread::Options io_thread_options;
  io_thread_options.message_pump_type = base::MessagePumpType::IO;
  base::Thread* io_thread = new base::Thread("CrAppShimIO");
  io_thread->StartWithOptions(std::move(io_thread_options));

  mojo::core::ScopedIPCSupport ipc_support(
      io_thread->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  // Initialize the NSApplication (and ensure that it was not previously
  // initialized).
  [AppShimApplication sharedApplication];
  CHECK([NSApp isKindOfClass:[AppShimApplication class]]);

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI,
                                                    /*is_main_thread=*/true);
  ui::WindowResizeHelperMac::Get()->Init(main_task_executor.task_runner());
  base::PlatformThread::SetName("CrAppShimMain");

  variations::InitCrashKeys();

  AppShimController::Params controller_params;
  controller_params.user_data_dir =
      base::PathService::CheckedGet(chrome::DIR_USER_DATA);
  // Extract profile path from user_data_dir (which contains app data path,
  // see BasicStartupComplete() above).
  // Ignore |info->profile_dir| because it is only the relative path (unless
  // it is empty, in which case this is a profile-agnostic app).
  if (!base::FilePath(app_mode_info_->profile_dir).empty()) {
    controller_params.profile_dir =
        base::FilePath(app_mode_info_->user_data_dir).DirName().DirName();
  }
  controller_params.app_id = app_mode_info_->app_mode_id;
  controller_params.app_name = base::UTF8ToUTF16(app_mode_info_->app_mode_name);
  controller_params.app_url = GURL(app_mode_info_->app_mode_url);
  controller_params.io_thread_runner = io_thread->task_runner();

  app_shim_controller_ = std::make_unique<AppShimController>(controller_params);

  base::RunLoop().Run();

  return 0;
}

bool AppShimMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  // Field trials and features are initialized in PreSandboxStartup.
  return false;
}

bool AppShimMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  // Mojo is initialized in PreSandboxStartup, since we need to make sure mojo
  // is configured as broker.
  return false;
}

bool AppShimMainDelegate::ShouldInitializePerfetto(InvokedIn invoked_in) {
  // Tracing is not currently initialized for app shims. If we were to start
  // initializing tracing we'd probably have to do it after features and field
  // trial state has been fully initialized, which is later in app shims.
  return false;
}

bool AppShimMainDelegate::ShouldReconfigurePartitionAlloc() {
  // PartitionAlloc shouldn't be reconfigured until we have the final feature
  // and field trial state from the chrome browser process.
  return false;
}

bool AppShimMainDelegate::ShouldLoadV8Snapshot(
    const std::string& process_type) {
  // V8 is not used in the app shim process.
  return false;
}

void AppShimMainDelegate::InitializeLocale() {
  // Calculate the preferred locale used by Chrome.
  // We can't use l10n_util::OverrideLocaleWithCocoaLocale() because it calls
  // [base::apple::OuterBundle() preferredLocalizations] which gets
  // localizations from the bundle of the running app (i.e. it is equivalent
  // to [[NSBundle mainBundle] preferredLocalizations]) instead of the
  // target bundle.
  NSArray<NSString*>* preferred_languages = NSLocale.preferredLanguages;
  NSArray<NSString*>* supported_languages =
      base::apple::OuterBundle().localizations;
  std::string preferred_localization;
  for (NSString* __strong language in preferred_languages) {
    // We must convert the "-" separator to "_" to be compatible with
    // NSBundle::localizations() e.g. "en-GB" becomes "en_GB".
    // See https://crbug.com/913345.
    language = [language stringByReplacingOccurrencesOfString:@"-"
                                                   withString:@"_"];
    if ([supported_languages containsObject:language]) {
      preferred_localization = base::SysNSStringToUTF8(language);
      break;
    }

    // For Chinese and Serbian, the preferred and supported languages don't
    // match due to script components and causes us to fall back to the next
    // matched language. e.g. Simplified Chinese is presented as 'zh_CN' in
    // supported_languages, but as 'zh_Hans_CN' in preferred_languages.
    // Instead of falling back, adjust those 3 language codes to match
    // language codes provided in supported_languages.
    if ([language hasPrefix:@"zh_Hans"]) {
      language = @"zh_CN";
    } else if ([language hasPrefix:@"zh_Hant"]) {
      language = @"zh_TW";
    } else if ([language hasPrefix:@"sr_Latn"]) {
      language = @"sr_Latn_RS";
    } else {
      // Check for language support without the region component.
      language = [language componentsSeparatedByString:@"_"][0];
    }

    if ([supported_languages containsObject:language]) {
      preferred_localization = base::SysNSStringToUTF8(language);
      break;
    }

    // Avoid defaulting to English or another unintended language when no
    // clear match is found. e.g. if there is no specific match for
    // "sr_Latn_RS" in supported_languages, it can at least fall back to a
    // generic Serbian language code ("sr").
    language = [language componentsSeparatedByString:@"_"][0];
    if ([supported_languages containsObject:language]) {
      preferred_localization = base::SysNSStringToUTF8(language);
      break;
    }
  }
  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(preferred_localization));

  // Load localized strings and mouse cursor images.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
}

content::ContentClient* AppShimMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<ChromeContentClient>();
  return content_client_.get();
}
