// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#import <Cocoa/Cocoa.h>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_logging.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/crash/content/app/crashpad.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

// The NSApplication for app shims is a vanilla NSApplication, but sub-class it
// so that we can DCHECK that we know precisely when it is initialized.
@interface AppShimApplication : NSApplication
@end

@implementation AppShimApplication
@end

extern "C" {

// |ChromeAppModeStart()| is the point of entry into the framework from the app
// mode loader. There are cases where the Chromium framework may have changed in
// a way that is incompatible with an older shim (e.g. change to libc++ library
// linking). The function name is versioned to provide a way to force shim
// upgrades if they are launched before an updated version of Chromium can
// upgrade them; the old shim will not be able to dyload the new
// ChromeAppModeStart, so it will fall back to the upgrade path. See
// https://crbug.com/561205.
__attribute__((visibility("default"))) int APP_SHIM_ENTRY_POINT_NAME(
    const app_mode::ChromeAppModeInfo* info);

}  // extern "C"

void PostRepeatingDelayedTask() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PostRepeatingDelayedTask),
      base::TimeDelta::FromDays(1));
}

int APP_SHIM_ENTRY_POINT_NAME(const app_mode::ChromeAppModeInfo* info) {
  base::CommandLine::Init(info->argc, info->argv);

  @autoreleasepool {
    base::AtExitManager exit_manager;
    chrome::RegisterPathProvider();

    // Set bundle paths. This loads the bundles.
    base::mac::SetOverrideOuterBundlePath(
        base::FilePath(info->chrome_outer_bundle_path));
    base::mac::SetOverrideFrameworkBundlePath(
        base::FilePath(info->chrome_framework_path));

    ChromeCrashReporterClient::Create();
    crash_reporter::InitializeCrashpad(true, "app_shim");

    // Calculate the preferred locale used by Chrome. We can't use
    // l10n_util::OverrideLocaleWithCocoaLocale() because it calls
    // [base::mac::OuterBundle() preferredLocalizations] which gets
    // localizations from the bundle of the running app (i.e. it is equivalent
    // to [[NSBundle mainBundle] preferredLocalizations]) instead of the target
    // bundle.
    NSArray* preferred_languages = [NSLocale preferredLanguages];
    NSArray* supported_languages = [base::mac::OuterBundle() localizations];
    std::string preferred_localization;
    for (NSString* language in preferred_languages) {
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
        locale, NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

    ChromeContentClient chrome_content_client;
    content::SetContentClient(&chrome_content_client);

    // Launch the IO thread.
    base::Thread::Options io_thread_options;
    io_thread_options.message_pump_type = base::MessagePumpType::IO;
    base::Thread* io_thread = new base::Thread("CrAppShimIO");
    io_thread->StartWithOptions(io_thread_options);

    mojo::core::Init();
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

    // TODO(https://crbug.com/925998): This workaround ensures that there is
    // always delayed work enqueued. If there is ever not enqueued delayed work,
    // then NSMenus and NSAlerts can start misbehaving (see
    // https://crbug.com/920795 for examples). This workaround is not an
    // appropriate solution to the problem, and should be replaced by a fix in
    // the relevant message pump code.
    PostRepeatingDelayedTask();

    AppShimController::Params controller_params;
    // Note that |info->user_data_dir| for shims contains the app data path,
    // <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/.
    controller_params.user_data_dir =
        base::FilePath(info->user_data_dir).DirName().DirName().DirName();
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

    AppShimController controller(controller_params);
    base::RunLoop().Run();
    return 0;
  }
}
