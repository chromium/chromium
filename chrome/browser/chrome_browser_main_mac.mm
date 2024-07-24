// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/buildflags.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/mac/install_from_dmg.h"
#include "chrome/browser/mac/metrics.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "chrome/browser/ui/cocoa/renderer_context_menu/chrome_swizzle_services_menu_updater.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "components/metrics/metrics_service.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/version_info/channel.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "net/cert/internal/system_trust_store.h"
#include "ui/base/cocoa/permissions_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/native_theme/native_theme_mac.h"

// ChromeBrowserMainPartsMac ---------------------------------------------------

namespace {

base::FilePath GetBundlePath() {
  if (!base::apple::AmIBundled()) {
    return base::FilePath();
  }
  return base::apple::OuterBundlePath();
}

base::FilePath GetMainExecutableName() {
  if (!base::apple::AmIBundled()) {
    return base::FilePath();
  }
  base::FilePath path;
  base::PathService::Get(base::FILE_EXE, &path);
  return path.BaseName();
}

}  // namespace

ChromeBrowserMainPartsMac::ChromeBrowserMainPartsMac(bool is_integration_test,
                                                     StartupData* startup_data)
    : ChromeBrowserMainPartsPosix(is_integration_test, startup_data),
      code_sign_clone_manager_(GetBundlePath(), GetMainExecutableName()) {}

ChromeBrowserMainPartsMac::~ChromeBrowserMainPartsMac() = default;

int ChromeBrowserMainPartsMac::PreEarlyInitialization() {
  if (base::mac::WasLaunchedAsLoginItemRestoreState()) {
    base::CommandLine* singleton_command_line =
        base::CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kRestoreLastSession);
  } else if (base::mac::WasLaunchedAsHiddenLoginItem()) {
    base::CommandLine* singleton_command_line =
        base::CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  [ChromeSwizzleServicesMenuUpdater install];

  return ChromeBrowserMainPartsPosix::PreEarlyInitialization();
}

void ChromeBrowserMainPartsMac::PreCreateMainMessageLoop() {
  ChromeBrowserMainPartsPosix::PreCreateMainMessageLoop();

  // ChromeBrowserMainParts should have loaded the resource bundle by this
  // point (needed to load the nib).
  CHECK(ui::ResourceBundle::HasSharedInstance());

#if !BUILDFLAG(CHROME_FOR_TESTING)
  // Disk image installation is sort of a first-run task, so it shares the
  // no first run switches.
  //
  // This needs to be done after the resource bundle is initialized (for
  // access to localizations in the UI) and after Keystone is initialized
  // (because the installation may need to promote Keystone) but before the
  // app controller is set up (and thus before MainMenu.nib is loaded, because
  // the app controller assumes that a browser has been set up and will crash
  // upon receipt of certain notifications if no browser exists), before
  // anyone tries doing anything silly like firing off an import job, and
  // before anything creating preferences like Local State in order for the
  // relaunched installed application to still consider itself as first-run.
  if (!first_run::IsFirstRunSuppressed(
          *base::CommandLine::ForCurrentProcess())) {
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      exit(0);
    }
  }
#endif  // !BUILDFLAG(CHROME_FOR_TESTING)

  // Create the app delegate by requesting the shared AppController.
  CHECK_EQ(nil, NSApp.delegate);
  AppController* app_controller = AppController.sharedController;
  CHECK_NE(nil, NSApp.delegate);

  chrome::BuildMainMenu(NSApp, app_controller,
                        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), false);
  [app_controller mainMenuCreated];

  ui::WarmScreenCapture();

  metrics_ = std::make_unique<mac_metrics::Metrics>();
  metrics_->RecordAppFileSystemType();

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // AppKit only restores windows to their original spaces when relaunching
  // apps after a restart, and puts them all on the current space when an app
  // is manually quit and relaunched. If Chrome restarted itself, ask AppKit to
  // treat this launch like a system restart and restore everything.
  if (local_state->GetBoolean(prefs::kWasRestarted)) {
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
      @"NSWindowRestoresWorkspaceAtLaunch" : @YES
    }];
  }
}

void ChromeBrowserMainPartsMac::PostCreateMainMessageLoop() {
  ChromeBrowserMainPartsPosix::PostCreateMainMessageLoop();

  net::InitializeTrustStoreMacCache();
}

void ChromeBrowserMainPartsMac::PreProfileInit() {
  ChromeBrowserMainPartsPosix::PreProfileInit();

  // This is called here so that the app shim socket is only created after
  // taking the singleton lock.
  g_browser_process->platform_part()->app_shim_listener()->Init();

  // Start up the platform auth SSO policy observer.
  if (auto* local_state = g_browser_process->local_state(); local_state) {
    platform_auth_policy_observer_ =
        std::make_unique<PlatformAuthPolicyObserver>(local_state);
  }
}

void ChromeBrowserMainPartsMac::PostProfileInit(Profile* profile,
                                                bool is_initial_profile) {
  ChromeBrowserMainPartsPosix::PostProfileInit(profile, is_initial_profile);
}

void ChromeBrowserMainPartsMac::PostMainMessageLoopRun() {
  // The `ProfileManager` has been destroyed, so no new platform authentication
  // requests will be created.
  platform_auth_policy_observer_.reset();

  ChromeBrowserMainParts::PostMainMessageLoopRun();
}

void ChromeBrowserMainPartsMac::DidEndMainMessageLoop() {
  [AppController.sharedController didEndMainMessageLoop];
}
