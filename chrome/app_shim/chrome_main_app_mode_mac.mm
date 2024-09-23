// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#import <Cocoa/Cocoa.h>

#include <utility>
#include <vector>

#include "base/allocator/early_zone_registration_apple.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_features.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

// The NSApplication for app shims is a vanilla NSApplication, but
// implements the CrAppProtocol and CrAppControlPrototocol protocols to skip
// creating an autorelease pool in nested event loops, for example when
// displaying a context menu.
@interface AppShimApplication
    : NSApplication <CrAppProtocol, CrAppControlProtocol>
@end

@implementation AppShimApplication {
  BOOL _handlingSendEvent;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (void)enableScreenReaderCompleteModeAfterDelay:(BOOL)enable {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (enableScreenReaderCompleteMode)
                                             object:nil];
  if (enable) {
    const float kTwoSecondDelay = 2.0;
    [self performSelector:@selector(enableScreenReaderCompleteMode)
               withObject:nil
               afterDelay:kTwoSecondDelay];
  }
}

- (void)enableScreenReaderCompleteMode {
  AppShimDelegate* delegate =
      base::apple::ObjCCastStrict<AppShimDelegate>(NSApp.delegate);
  [delegate enableAccessibilitySupport:
                chrome::mojom::AppShimScreenReaderSupportMode::kComplete];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  // This is an undocumented attribute that's set when VoiceOver is turned
  // on/off or Text To Speech is triggered. In addition, some apps use it to
  // request accessibility activation.
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"]) {
    // `sonomaAccessibilityRefinementsAreActive` has the same purpose with
    // BrowserCrApplication. See chrome_browser_application_mac.mm to learn
    // more.
    BOOL sonomaAccessibilityRefinementsAreActive =
        base::mac::MacOSVersion() >= 14'00'00 &&
        base::FeatureList::IsEnabled(
            features::kSonomaAccessibilityActivationRefinements);
    // When there are ATs that want to access this PWA app's accessibility, we
    // need to notify browser proces to enable accessibility. When ATs no
    // longer need access to this PWA app's accessibility, we don't want it to
    // affect the browser in case other PWA apps or the browser itself still
    // need to use accessbility.
    if (sonomaAccessibilityRefinementsAreActive) {
      [self enableScreenReaderCompleteModeAfterDelay:[value boolValue]];
    } else {
      if ([value boolValue]) {
        [self enableScreenReaderCompleteMode];
      }
    }
  }
  return [super accessibilitySetValue:value forAttribute:attribute];
}

- (NSAccessibilityRole)accessibilityRole {
  AppShimDelegate* delegate =
      base::apple::ObjCCastStrict<AppShimDelegate>(NSApp.delegate);
  [delegate enableAccessibilitySupport:
                chrome::mojom::AppShimScreenReaderSupportMode::kPartial];
  return [super accessibilityRole];
}

@end

extern "C" {
// |ChromeAppModeStart()| is the point of entry into the framework from the
// app mode loader. There are cases where the Chromium framework may have
// changed in a way that is incompatible with an older shim (e.g. change to
// libc++ library linking). The function name is versioned to provide a way
// to force shim upgrades if they are launched before an updated version of
// Chromium can upgrade them; the old shim will not be able to dyload the
// new ChromeAppModeStart, so it will fall back to the upgrade path. See
// https://crbug.com/561205.
__attribute__((visibility("default"))) int APP_SHIM_ENTRY_POINT_NAME(
    const app_mode::ChromeAppModeInfo* info);

}  // extern "C"

int APP_SHIM_ENTRY_POINT_NAME(const app_mode::ChromeAppModeInfo* info) {
  // The static constructor in //base will have registered PartitionAlloc as
  // the default zone. Allow the //base instance in the main library to
  // register it as well. Otherwise we end up passing memory to free() which
  // was allocated by an unknown zone. See crbug.com/1274236 for details.
  partition_alloc::AllowDoublePartitionAllocZoneRegistration();

  base::CommandLine::Init(info->argc, info->argv);

  @autoreleasepool {
    base::AtExitManager exit_manager;
    chrome::RegisterPathProvider();

    // Set bundle paths. This loads the bundles.
    base::apple::SetOverrideOuterBundlePath(
        base::FilePath(info->chrome_outer_bundle_path));
    base::apple::SetOverrideFrameworkBundlePath(
        base::FilePath(info->chrome_framework_path));

    // Note that `info->user_data_dir` for shims contains the app data path,
    // <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/.
    const base::FilePath user_data_dir =
        base::FilePath(info->user_data_dir).DirName().DirName().DirName();

    // TODO(crbug.com/40807881): Specify `user_data_dir` to  CrashPad.
    ChromeCrashReporterClient::Create();
    crash_reporter::InitializeCrashpad(true, "app_shim");

    base::PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_USER_DATA, user_data_dir, /*is_absolute=*/false,
        /*create=*/false);

    // Initialize features and field trials, either from command line or from
    // file in user data dir.
    AppShimController::PreInitFeatureState(
        *base::CommandLine::ForCurrentProcess());

    // Calculate the preferred locale used by Chrome. We can't use
    // l10n_util::OverrideLocaleWithCocoaLocale() because it calls
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
      // Check for language support without the region component.
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

    ChromeContentClient chrome_content_client;
    content::SetContentClient(&chrome_content_client);

    // Local histogram to let tests verify that histograms are emitted properly.
    LOCAL_HISTOGRAM_BOOLEAN("AppShim.Launched", true);

    // Launch the IO thread.
    base::Thread::Options io_thread_options;
    io_thread_options.message_pump_type = base::MessagePumpType::IO;
    base::Thread* io_thread = new base::Thread("CrAppShimIO");
    io_thread->StartWithOptions(std::move(io_thread_options));

    // It's necessary to call Mojo's InitFeatures() to ensure we're using the
    // same IPC implementation as the browser.
    mojo::core::InitFeatures();

    // Create a ThreadPool, but don't start it yet until we have fully
    // initialized base::Feature and field trial support.
    base::ThreadPoolInstance::Create("AppShim");

    // We're using an isolated Mojo connection between the browser and this
    // process, so this process must act as a broker.
    mojo::core::Configuration config;
    config.is_broker_process = true;
    mojo::core::Init(config);
    mojo::core::ScopedIPCSupport ipc_support(
        io_thread->task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

    // Initialize the NSApplication (and ensure that it was not previously
    // initialized).
    [AppShimApplication sharedApplication];
    CHECK([NSApp isKindOfClass:[AppShimApplication class]]);

    base::SingleThreadTaskExecutor main_task_executor(
        base::MessagePumpType::UI);
    ui::WindowResizeHelperMac::Get()->Init(main_task_executor.task_runner());
    base::PlatformThread::SetName("CrAppShimMain");

    AppShimController::Params controller_params;
    controller_params.user_data_dir = user_data_dir;
    // Similarly, extract the full profile path from |info->user_data_dir|.
    // Ignore |info->profile_dir| because it is only the relative path (unless
    // it is empty, in which case this is a profile-agnostic app).
    if (!base::FilePath(info->profile_dir).empty()) {
      controller_params.profile_dir =
          base::FilePath(info->user_data_dir).DirName().DirName();
    }
    controller_params.app_id = info->app_mode_id;
    controller_params.app_name = base::UTF8ToUTF16(info->app_mode_name);
    controller_params.app_url = GURL(info->app_mode_url);
    controller_params.io_thread_runner = io_thread->task_runner();

    AppShimController controller(controller_params);
    base::RunLoop().Run();
    return 0;
  }
}
