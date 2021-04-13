// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include <dispatch/dispatch.h>
#include <stddef.h>

#include <memory>
#include <vector>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/mac/auth_session_request.h"
#include "chrome/browser/mac/mac_startup_profiler.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_mac.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/confirm_quit.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "chrome/browser/ui/cocoa/handoff_active_url_observer_bridge.h"
#import "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/profiles/profile_menu_controller.h"
#import "chrome/browser/ui/cocoa/share_menu_controller.h"
#import "chrome/browser/ui/cocoa/tab_menu_bridge.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/handoff/handoff_manager.h"
#include "components/handoff/handoff_utility.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "net/base/filename_util.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/cocoa/focus_window_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

using apps::AppShimManager;
using base::UserMetricsAction;
using content::BrowserContext;
using content::DownloadManager;

namespace {

// How long we allow a workspace change notification to wait to be
// associated with a dock activation. The animation lasts 250ms. See
// applicationShouldHandleReopen:hasVisibleWindows:.
static const int kWorkspaceChangeTimeoutMs = 500;

// True while AppController is calling chrome::NewEmptyWindow(). We need a
// global flag here, analogue to StartupBrowserCreator::InProcessStartup()
// because otherwise the SessionService will try to restore sessions when we
// make a new window while there are no other active windows.
bool g_is_opening_new_window = false;

// Activates a browser window having the given profile (the last one active) if
// possible and returns a pointer to the activate |Browser| or NULL if this was
// not possible. If the last active browser is minimized (in particular, if
// there are only minimized windows), it will unminimize it.
Browser* ActivateBrowser(Profile* profile) {
  Browser* browser = chrome::FindLastActiveWithProfile(
      profile->IsGuestSession() ? profile->GetPrimaryOTRProfile() : profile);
  if (browser)
    browser->window()->Activate();
  return browser;
}

// Creates an empty browser window with the given profile and returns a pointer
// to the new |Browser|.
Browser* CreateBrowser(Profile* profile) {
  {
    base::AutoReset<bool> auto_reset_in_run(&g_is_opening_new_window, true);
    chrome::NewEmptyWindow(profile);
  }

  Browser* browser = chrome::GetLastActiveBrowser();
  CHECK(browser);
  return browser;
}

// Activates a browser window having the given profile (the last one active) if
// possible or creates an empty one if necessary. Returns a pointer to the
// activated/new |Browser|.
Browser* ActivateOrCreateBrowser(Profile* profile) {
  if (Browser* browser = ActivateBrowser(profile))
    return browser;
  return CreateBrowser(profile);
}

CFStringRef BaseBundleID_CFString() {
  NSString* base_bundle_id =
      [NSString stringWithUTF8String:base::mac::BaseBundleID()];
  return base::mac::NSToCFCast(base_bundle_id);
}

// Record the location of the application bundle (containing the main framework)
// from which Chromium was loaded. This is used by app mode shims to find
// Chromium.
void RecordLastRunAppBundlePath() {
  // Going up three levels from |chrome::GetVersionedDirectory()| gives the
  // real, user-visible app bundle directory. (The alternatives give either the
  // framework's path or the initial app's path, which may be an app mode shim
  // or a unit test.)
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Go up five levels from the versioned sub-directory of the framework, which
  // is at C.app/Contents/Frameworks/C.framework/Versions/V.
  base::FilePath app_bundle_path = chrome::GetFrameworkBundlePath()
                                       .DirName()
                                       .DirName()
                                       .DirName()
                                       .DirName()
                                       .DirName();
  base::ScopedCFTypeRef<CFStringRef> app_bundle_path_cfstring(
      base::SysUTF8ToCFStringRef(app_bundle_path.value()));
  CFPreferencesSetAppValue(
      base::mac::NSToCFCast(app_mode::kLastRunAppBundlePathPrefsKey),
      app_bundle_path_cfstring, BaseBundleID_CFString());
}

bool IsProfileSignedOut(Profile* profile) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->IsSigninRequired();
}

void ConfigureNSAppForKioskMode() {
  NSApp.presentationOptions =
      NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar |
      NSApplicationPresentationDisableProcessSwitching |
      NSApplicationPresentationDisableSessionTermination |
      NSApplicationPresentationDisableForceQuit |
      NSApplicationPresentationFullScreen;
}

}  // namespace

// Returns the last profile. This is extracted as a standalone function in order
// to be friend with base::ScopedAllowBlocking.
Profile* GetLastProfileMac() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  base::FilePath profile_path =
      GetStartupProfilePath(profile_manager->user_data_dir(),
                            /*current_directory=*/base::FilePath(),
                            *base::CommandLine::ForCurrentProcess(),
                            /*ignore_profile_picker=*/true);

  // ProfileManager::GetProfile() is blocking if the profile was not loaded yet.
  // TODO(https://1176734): Change this code to return nullptr when the profile
  // is not loaded, and update all callers to handle this case.
  base::ScopedAllowBlocking allow_blocking;

  // lastProfile is used to open URLs passed in application:openFiles: and
  // should not default to Guest, even if the profile picker is shown.
  // TODO(https://crbug.com/1155158): Remove the ignore_profile_picker parameter
  // once the picker supports opening URLs.
  return profile_manager->GetProfile(profile_path);
}

@interface AppController () <HandoffActiveURLObserverBridgeDelegate>
- (void)initMenuState;
- (void)initProfileMenu;
- (void)updateConfirmToQuitPrefMenuItem:(NSMenuItem*)item;
- (void)registerServicesMenuTypesTo:(NSApplication*)app;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
- (void)activeSpaceDidChange:(NSNotification*)inNotification;
- (void)checkForAnyKeyWindows;
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount;
- (BOOL)shouldQuitWithInProgressDownloads;
- (void)executeApplication:(id)sender;
- (void)profileWasRemoved:(const base::FilePath&)profilePath;
- (void)setLastProfile:(Profile*)profile;

// Opens a tab for each GURL in |urls|.
- (void)openUrls:(const std::vector<GURL>&)urls;

// This class cannot open urls until startup has finished. The urls that cannot
// be opened are cached in |startupUrls_|. This method must be called exactly
// once after startup has completed. It opens the urls in |startupUrls_|, and
// clears |startupUrls_|.
- (void)openStartupUrls;

// Opens a tab for each GURL in |urls|. If there is exactly one tab open before
// this method is called, and that tab is the NTP, then this method closes the
// NTP after all the |urls| have been opened.
- (void)openUrlsReplacingNTP:(const std::vector<GURL>&)urls;

// This method passes |handoffURL| to |handoffManager_|.
- (void)passURLToHandoffManager:(const GURL&)handoffURL;

// Lazily creates the Handoff Manager. Updates the state of the Handoff
// Manager. This method is idempotent. This should be called:
// - During initialization.
// - When the current tab navigates to a new URL.
// - When the active browser changes.
// - When the active browser's active tab switches.
// |webContents| should be the new, active WebContents.
- (void)updateHandoffManager:(content::WebContents*)webContents;

// Given |webContents|, extracts a GURL to be used for Handoff. This may return
// the empty GURL.
- (GURL)handoffURLFromWebContents:(content::WebContents*)webContents;

// Return false if Chrome startup is paused by dialog and AppController is
// called without any initialized Profile.
- (BOOL)isProfileReady;
@end

class AppControllerProfileObserver : public ProfileAttributesStorage::Observer,
                                     public ProfileManagerObserver,
                                     public ProfileObserver {
 public:
  AppControllerProfileObserver(
      ProfileManager* profile_manager, AppController* app_controller)
      : profile_manager_(profile_manager),
        app_controller_(app_controller) {
    DCHECK(profile_manager_);
    DCHECK(app_controller_);
    // Listen to different events, depending on whether the
    // kDestroyProfileOnBrowserClose experiment is disabled or not.
    if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
      profile_manager_->AddObserver(this);
      for (Profile* profile : profile_manager_->GetLoadedProfiles())
        profile->AddObserver(this);
    } else {
      profile_manager_->GetProfileAttributesStorage().AddObserver(this);
    }
  }

  ~AppControllerProfileObserver() override {
    DCHECK(profile_manager_);
    if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
      profile_manager_->RemoveObserver(this);
    } else {
      profile_manager_->GetProfileAttributesStorage().RemoveObserver(this);
    }
  }

 private:
  // ProfileAttributesStorage::Observer implementation:
  void OnProfileAdded(const base::FilePath& profile_path) override {}

  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override {
    // When a profile is deleted we need to notify the AppController,
    // so it can correctly update its pointer to the last used profile.
    [app_controller_ profileWasRemoved:profile_path];
  }

  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override {}

  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override {}

  void OnProfileAvatarChanged(const base::FilePath& profile_path) override {}

  // ProfileManager::Observer implementation:
  void OnProfileAdded(Profile* profile) override { profile->AddObserver(this); }

  // ProfileObserver implementation:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    profile->RemoveObserver(this);
    [app_controller_ profileWasRemoved:profile->GetPath()];
  }

  ProfileManager* profile_manager_;

  AppController* app_controller_;  // Weak; owns us.

  DISALLOW_COPY_AND_ASSIGN(AppControllerProfileObserver);
};

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
// On macOS 10.12, the IME system attempts to allocate a 2^64 size buffer,
// which would typically cause an OOM crash. To avoid this, the problematic
// method is swizzled out and the make-OOM-fatal bit is disabled for the
// duration of the original call. https://crbug.com/654695
static base::mac::ScopedObjCClassSwizzler* g_swizzle_imk_input_session;

@interface OOMDisabledIMKInputSession : NSObject
@end

@implementation OOMDisabledIMKInputSession

- (void)_coreAttributesFromRange:(NSRange)range
                 whichAttributes:(long long)attributes
               completionHandler:(void (^)(void))block {
  // The allocator flag is per-process, so other threads may temporarily
  // not have fatal OOM occur while this method executes, but it is better
  // than crashing when using IME.
  base::allocator::SetCallNewHandlerOnMallocFailure(false);
  g_swizzle_imk_input_session
      ->InvokeOriginal<void, NSRange, long long, void (^)(void)>(
          self, _cmd, range, attributes, block);
  base::allocator::SetCallNewHandlerOnMallocFailure(true);
}

@end
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

@implementation AppController

@synthesize startupComplete = _startupComplete;

- (void)dealloc {
  [[_closeTabMenuItem menu] setDelegate:nil];
  [super dealloc];
}

// This method is called very early in application startup (ie, before
// the profile is loaded or any preferences have been registered). Defer any
// user-data initialization until -applicationDidFinishLaunching:.
- (void)mainMenuCreated {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::AWAKE_FROM_NIB);
  // We need to register the handlers early to catch events fired on launch.
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:'WWW!'    // A particularly ancient AppleEvent that dates
           andEventID:'OURL'];  // back to the Spyglass days.

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter
      addObserver:self
         selector:@selector(windowDidResignKey:)
             name:NSWindowDidResignKeyNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowDidBecomeMain:)
             name:NSWindowDidBecomeMainNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowDidResignMain:)
             name:NSWindowDidResignMainNotification
           object:nil];

  // Register for space change notifications.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
    addObserver:self
       selector:@selector(activeSpaceDidChange:)
           name:NSWorkspaceActiveSpaceDidChangeNotification
         object:nil];

  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserver:self
         selector:@selector(willPowerOff:)
             name:NSWorkspaceWillPowerOffNotification
           object:nil];

  NSMenu* fileMenu = [[[NSApp mainMenu] itemWithTag:IDC_FILE_MENU] submenu];
  _closeTabMenuItem = [fileMenu itemWithTag:IDC_CLOSE_TAB];
  DCHECK(_closeTabMenuItem);
  _closeWindowMenuItem = [fileMenu itemWithTag:IDC_CLOSE_WINDOW];
  DCHECK(_closeWindowMenuItem);

  // Set up the command updater for when there are no windows open
  [self initMenuState];

  // Initialize the Profile menu.
  [self initProfileMenu];
}

- (void)unregisterEventHandlers {
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
  [em removeEventHandlerForEventClass:'WWW!'
                           andEventID:'OURL'];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

// (NSApplicationDelegate protocol) This is the Apple-approved place to override
// the default handlers.
- (void)applicationWillFinishLaunching:(NSNotification*)notification {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::WILL_FINISH_LAUNCHING);

  if (@available(macOS 10.12, *)) {
    NSWindow.allowsAutomaticWindowTabbing = NO;
  }

  // If the OSX version supports this method, the system will automatically
  // hide the item if there's no touch bar. However, for unsupported versions,
  // we'll have to manually remove the item from the menu.
  if (![NSApp
          respondsToSelector:@selector(toggleTouchBarCustomizationPalette:)]) {
    NSMenu* mainMenu = [NSApp mainMenu];
    NSMenu* viewMenu = [[mainMenu itemWithTag:IDC_VIEW_MENU] submenu];
    NSMenuItem* customizeItem = [viewMenu itemWithTag:IDC_CUSTOMIZE_TOUCH_BAR];
    if (customizeItem)
      [viewMenu removeItem:customizeItem];
  }

  [self initShareMenu];
}

- (BOOL)tryToTerminateApplication:(NSApplication*)app {
  // Reset this now that we've received the call to terminate.
  BOOL isPoweringOff = _isPoweringOff;
  _isPoweringOff = NO;

  // Check for in-process downloads, and prompt the user if they really want
  // to quit (and thus cancel downloads). Only check if we're not already
  // shutting down, else the user might be prompted multiple times if the
  // download isn't stopped before terminate is called again.
  if (!browser_shutdown::IsTryingToQuit() &&
      ![self shouldQuitWithInProgressDownloads])
    return NO;

  // TODO(viettrungluu): Remove Apple Event handlers here? (It's safe to leave
  // them in, but I'm not sure about UX; we'd also want to disable other things
  // though.) http://crbug.com/40861

  // Check for active apps. If quitting is prevented, only close browsers and
  // sessions.
  if (!browser_shutdown::IsTryingToQuit() && !isPoweringOff &&
      _quitWithAppsController.get() && !_quitWithAppsController->ShouldQuit()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kHostedAppQuitNotification)) {
      return NO;
    }

    chrome::OnClosingAllBrowsers(true);
    // This will close all browser sessions.
    chrome::CloseAllBrowsers();

    // At this point, the user has already chosen to cancel downloads. If we
    // were to shut down as usual, the downloads would be cancelled in
    // DownloadCoreService::Shutdown().
    DownloadCoreService::CancelAllDownloads();

    return NO;
  }

  size_t num_browsers = chrome::GetTotalBrowserCount();

  // Initiate a shutdown (via chrome::CloseAllBrowsersAndQuit()) if we aren't
  // already shutting down.
  if (!browser_shutdown::IsTryingToQuit()) {
    chrome::OnClosingAllBrowsers(true);
    chrome::CloseAllBrowsersAndQuit();
  }

  return num_browsers == 0 ? YES : NO;
}

- (void)stopTryingToTerminateApplication:(NSApplication*)app {
  if (browser_shutdown::IsTryingToQuit()) {
    // Reset the "trying to quit" state, so that closing all browser windows
    // will no longer lead to termination.
    browser_shutdown::SetTryingToQuit(false);

    // TODO(viettrungluu): Were we to remove Apple Event handlers above, we
    // would have to reinstall them here. http://crbug.com/40861
  }
}

- (BOOL)runConfirmQuitPanel {
  // If there are no windows, quit immediately.
  if (BrowserList::GetInstance()->empty() &&
      !AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0)) {
    return YES;
  }

  // Check if the preference is turned on.
  const PrefService* prefs = g_browser_process->local_state();
  if (!prefs->GetBoolean(prefs::kConfirmToQuitEnabled)) {
    confirm_quit::RecordHistogram(confirm_quit::kNoConfirm);
    return YES;
  }

  // Run only for keyboard-initiated quits.
  if ([[NSApp currentEvent] type] != NSKeyDown)
    return NSTerminateNow;

  return [[ConfirmQuitPanelController sharedController]
      runModalLoopForApplication:NSApp];
}

// Called when the app is shutting down. Clean-up as appropriate.
- (void)applicationWillTerminate:(NSNotification*)aNotification {
  // There better be no browser windows left at this point.
  CHECK_EQ(0u, chrome::GetTotalBrowserCount());

  // Tell BrowserList not to keep the browser process alive. Once all the
  // browsers get dealloc'd, it will stop the RunLoop and fall back into main().
  _keep_alive.reset();

  // Reset all pref watching, as this object outlives the prefs system.
  _profilePrefRegistrar.reset();
  _localPrefRegistrar.RemoveAll();

  // It's safe to delete |_lastProfile| now.
  [self setLastProfile:nullptr];

  [self unregisterEventHandlers];

  _appShimMenuController.reset();

  _profileBookmarkMenuBridgeMap.clear();
}

- (void)didEndMainMessageLoop {
  if (!_lastProfile) {
    // If only the profile picker is open and closed again, there is no profile
    // loaded when main message loop ends and we cannot load it from disk now.
    return;
  }
  DCHECK_EQ(0u, chrome::GetBrowserCount(_lastProfile));
  if (!chrome::GetBrowserCount(_lastProfile)) {
    // As we're shutting down, we need to nuke the TabRestoreService, which
    // will start the shutdown of the NavigationControllers and allow for
    // proper shutdown. If we don't do this, Chrome won't shut down cleanly,
    // and may end up crashing when some thread tries to use the IO thread (or
    // another thread) that is no longer valid.
    TabRestoreServiceFactory::ResetForProfile(_lastProfile);
  }
}

// If the window has a tab controller, make "close window" be cmd-shift-w,
// otherwise leave it as the normal cmd-w. Capitalization of the key equivalent
// affects whether the shift modifier is used.
- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)enableCloseTabShortcut {
  [_closeWindowMenuItem setKeyEquivalent:(enableCloseTabShortcut ? @"W" :
                                                                   @"w")];
  [_closeWindowMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
}

// If the window has a tab controller, make "close tab" take over cmd-w,
// otherwise it shouldn't have any key-equivalent because it should be disabled.
- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)enableCloseTabShortcut {
  if (enableCloseTabShortcut) {
    [_closeTabMenuItem setKeyEquivalent:@"w"];
    [_closeTabMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
  } else {
    [_closeTabMenuItem setKeyEquivalent:@""];
    [_closeTabMenuItem setKeyEquivalentModifierMask:0];
  }
}

// See if the focused window window has tabs, and adjust the key equivalents for
// Close Tab/Close Window accordingly.
- (void)menuNeedsUpdate:(NSMenu*)menu {
  DCHECK(menu == [_closeTabMenuItem menu]);
  [self updateMenuItemKeyEquivalents];
}

- (void)windowDidResignKey:(NSNotification*)notify {
  // If a window is closed, this notification is fired but |[NSApp keyWindow]|
  // returns nil regardless of whether any suitable candidates for the key
  // window remain. It seems that the new key window for the app is not set
  // until after this notification is fired, so a check is performed after the
  // run loop is allowed to spin.
  [self performSelector:@selector(checkForAnyKeyWindows)
             withObject:nil
             afterDelay:0.0];
}

- (void)windowDidBecomeMain:(NSNotification*)notify {
  Browser* browser = chrome::FindBrowserWithWindow([notify object]);
  if (!browser)
    return;

  if (browser->is_type_normal()) {
    _tabMenuBridge = std::make_unique<TabMenuBridge>(
        browser->tab_strip_model(),
        [[NSApp mainMenu] itemWithTag:IDC_TAB_MENU]);
    _tabMenuBridge->BuildMenu();
  } else {
    _tabMenuBridge.reset();
  }

  [self windowChangedToProfile:browser->profile()->GetOriginalProfile()];
}

- (void)windowDidResignMain:(NSNotification*)notify {
  if (_lastProfile && chrome::GetTotalBrowserCount() == 0 &&
      [self isProfileReady]) {
    [self windowChangedToProfile:_lastProfile];
  }
}

- (void)activeSpaceDidChange:(NSNotification*)notify {
  if (_reopenTime.is_null() ||
      ![NSApp isActive] ||
      (base::TimeTicks::Now() - _reopenTime).InMilliseconds() >
      kWorkspaceChangeTimeoutMs) {
    return;
  }

  // The last applicationShouldHandleReopen:hasVisibleWindows: call
  // happened during a space change. Now that the change has
  // completed, raise browser windows.
  _reopenTime = base::TimeTicks();
  std::set<gfx::NativeWindow> browserWindows;
  for (auto* browser : *BrowserList::GetInstance())
    browserWindows.insert(browser->window()->GetNativeWindow());
  if (!browserWindows.empty())
    ui::FocusWindowSetOnCurrentSpace(browserWindows);
}

// Called when shutting down or logging out.
- (void)willPowerOff:(NSNotification*)notify {
  // Don't attempt any shutdown here. Cocoa will shortly call
  // -[BrowserCrApplication terminate:].
  _isPoweringOff = YES;
}

- (void)checkForAnyKeyWindows {
  if ([NSApp keyWindow])
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_NO_KEY_WINDOW,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

// If the auto-update interval is not set, make it 5 hours.
// Placed here for 2 reasons:
// 1) Same spot as other Pref stuff
// 2) Try and be friendly by keeping this after app launch
- (void)setUpdateCheckInterval {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  CFStringRef app = CFSTR("com.google.Keystone.Agent");
  CFStringRef checkInterval = CFSTR("checkInterval");
  CFPropertyListRef plist = CFPreferencesCopyAppValue(checkInterval, app);
  if (!plist) {
    const float fiveHoursInSeconds = 5.0 * 60.0 * 60.0;
    NSNumber* value = [NSNumber numberWithFloat:fiveHoursInSeconds];
    CFPreferencesSetAppValue(checkInterval, value, app);
    CFPreferencesAppSynchronize(app);
  }
#endif
}

- (void)openStartupUrls {
  DCHECK(_startupComplete);
  [self openUrlsReplacingNTP:_startupUrls];
  _startupUrls.clear();
}

- (void)openUrlsReplacingNTP:(const std::vector<GURL>&)urls {
  if (urls.empty())
    return;

  // On Mac, the URLs are passed in via Cocoa, not command line. The Chrome
  // NSApplication is created in MainMessageLoop, and then the shortcut urls
  // are passed in via Apple events. At this point, the first browser is
  // already loaded in PreMainMessageLoop. If we initialize NSApplication
  // before PreMainMessageLoop to capture shortcut URL events, it may cause
  // more problems because it relies on things created in PreMainMessageLoop
  // and may break existing message loop design.

  // If the browser hasn't started yet, just queue up the URLs.
  if (!_startupComplete) {
    _startupUrls.insert(_startupUrls.end(), urls.begin(), urls.end());
    return;
  }

  // If there's only 1 tab and the tab is NTP, close this NTP tab and open all
  // startup urls in new tabs, because the omnibox will stay focused if we
  // load url in NTP tab.
  Profile* profile =
      g_browser_process->profile_manager()->GetLastUsedProfileAllowedByPolicy();
  Browser* browser = chrome::FindLastActiveWithProfile(profile);

  int startupIndex = TabStripModel::kNoTab;
  content::WebContents* startupContent = NULL;

  if (browser && browser->tab_strip_model()->count() == 1) {
    startupIndex = browser->tab_strip_model()->active_index();
    startupContent = browser->tab_strip_model()->GetActiveWebContents();
  }

  [self openUrls:urls];

  // This NTP check should be replaced once https://crbug.com/624410 is fixed.
  if (startupIndex != TabStripModel::kNoTab &&
      (startupContent->GetVisibleURL() == chrome::kChromeUINewTabURL ||
       startupContent->GetVisibleURL() == chrome::kChromeUINewTabPageURL)) {
    browser->tab_strip_model()->CloseWebContentsAt(startupIndex,
        TabStripModel::CLOSE_NONE);
  }
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidFinishLaunching:(NSNotification*)notify {
  if (g_browser_process->browser_policy_connector()
          ->chrome_browser_cloud_management_controller()
          ->IsEnterpriseStartupDialogShowing()) {
    // As Chrome is not ready when the Enterprise startup dialog is being shown.
    // Store the notification as it will be reposted when the dialog is closed.
    return;
  }

  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::DID_FINISH_LAUNCHING);
  MacStartupProfiler::GetInstance()->RecordMetrics();

  // Notify BrowserList to keep the application running so it doesn't go away
  // when all the browser windows get closed.
  _keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::APP_CONTROLLER, KeepAliveRestartOption::DISABLED);

  [self setUpdateCheckInterval];

  // Start managing the menu for app windows. This needs to be done here because
  // main menu item titles are not yet initialized in awakeFromNib.
  [self initAppShimMenuController];

  // If enabled, keep Chrome alive when apps are open instead of quitting all
  // apps.
  _quitWithAppsController = new QuitWithAppsController();

  // Dynamically update shortcuts for "Close Window" and "Close Tab" menu items.
  [[_closeTabMenuItem menu] setDelegate:self];

  // Instantiate the ProfileAttributesStorage observer so that we can get
  // notified when a profile is deleted.
  _profileAttributesStorageObserver =
      std::make_unique<AppControllerProfileObserver>(
          g_browser_process->profile_manager(), self);

  // Record the path to the (browser) app bundle; this is used by the app mode
  // shim.
  if (base::mac::AmIBundled()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&RecordLastRunAppBundlePath));
  }

  // Makes "Services" menu items available.
  [self registerServicesMenuTypesTo:[notify object]];

  _startupComplete = YES;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    ConfigureNSAppForKioskMode();

  Browser* browser = chrome::FindLastActive();
  content::WebContents* activeWebContents = nullptr;
  if (browser)
    activeWebContents = browser->tab_strip_model()->GetActiveWebContents();
  [self updateHandoffManager:activeWebContents];
  [self openStartupUrls];

  PrefService* localState = g_browser_process->local_state();
  if (localState) {
    _localPrefRegistrar.Init(localState);
    _localPrefRegistrar.Add(
        prefs::kAllowFileSelectionDialogs,
        base::BindRepeating(
            &chrome::BrowserCommandController::UpdateOpenFileState,
            _menuState.get()));
  }

  _handoff_active_url_observer_bridge =
      std::make_unique<HandoffActiveURLObserverBridge>(self);

  if (@available(macOS 10.15, *)) {
    ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
        .sessionHandler = self;
  }

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  // Disable fatal OOM to hack around an OS bug https://crbug.com/654695.
  if (base::mac::IsOS10_12()) {
    g_swizzle_imk_input_session = new base::mac::ScopedObjCClassSwizzler(
        NSClassFromString(@"IMKInputSession"),
        [OOMDisabledIMKInputSession class],
        @selector(_coreAttributesFromRange:whichAttributes:completionHandler:));
  }
#endif
}

// Helper function for populating and displaying the in progress downloads at
// exit alert panel.
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount {
  NSString* titleText = nil;
  NSString* explanationText = nil;
  NSString* waitTitle = nil;
  NSString* exitTitle = nil;

  // Set the dialog text based on whether or not there are multiple downloads.
  // Dialog text: warning and explanation.
  titleText = l10n_util::GetPluralNSStringF(IDS_ABANDON_DOWNLOAD_DIALOG_TITLE,
                                            downloadCount);
  explanationText =
      l10n_util::GetNSString(IDS_ABANDON_DOWNLOAD_DIALOG_BROWSER_MESSAGE);
  // "Cancel download and exit" button text.
  exitTitle = l10n_util::GetNSString(IDS_ABANDON_DOWNLOAD_DIALOG_EXIT_BUTTON);

  // "Wait for download" button text.
  waitTitle =
      l10n_util::GetNSString(IDS_ABANDON_DOWNLOAD_DIALOG_CONTINUE_BUTTON);

  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:titleText];
  [alert setInformativeText:explanationText];
  [alert addButtonWithTitle:waitTitle];
  [alert addButtonWithTitle:exitTitle];

  // 'waitButton' is the default choice.
  int choice = [alert runModal];
  return choice == NSAlertFirstButtonReturn ? YES : NO;
}

// Check all profiles for in progress downloads, and if we find any, prompt the
// user to see if we should continue to exit (and thus cancel the downloads), or
// if we should wait.
- (BOOL)shouldQuitWithInProgressDownloads {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return YES;

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());

  std::vector<Profile*> added_profiles;
  for (Profile* p : profiles) {
    for (Profile* otr : p->GetAllOffTheRecordProfiles())
      added_profiles.push_back(otr);
  }
  profiles.insert(profiles.end(), added_profiles.begin(), added_profiles.end());

  for (size_t i = 0; i < profiles.size(); ++i) {
    DownloadCoreService* download_core_service =
        DownloadCoreServiceFactory::GetForBrowserContext(profiles[i]);
    DownloadManager* download_manager =
        (download_core_service->HasCreatedDownloadManager()
             ? BrowserContext::GetDownloadManager(profiles[i])
             : NULL);
    if (download_manager &&
        download_manager->NonMaliciousInProgressCount() > 0) {
      int downloadCount = download_manager->NonMaliciousInProgressCount();
      if ([self userWillWaitForInProgressDownloads:downloadCount]) {
        // Create a new browser window (if necessary) and navigate to the
        // downloads page if the user chooses to wait.
        Browser* browser = chrome::FindBrowserWithProfile(profiles[i]);
        if (!browser) {
          browser = Browser::Create(Browser::CreateParams(profiles[i], true));
          browser->window()->Show();
        }
        DCHECK(browser);
        chrome::ShowDownloads(browser);
        return NO;
      }

      // User wants to exit.
      return YES;
    }
  }

  // No profiles or active downloads found, okay to exit.
  return YES;
}

// Called to determine if we should enable the "restore tab" menu item.
// Checks with the TabRestoreService to see if there's anything there to
// restore and returns YES if so.
- (BOOL)canRestoreTab {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile([self lastProfile]);
  return service && !service->entries().empty();
}

// Called from the AppControllerProfileObserver every time a profile is deleted.
- (void)profileWasRemoved:(const base::FilePath&)profilePath {
  // If the lastProfile has been deleted, the profile manager has
  // already loaded a new one, so the pointer needs to be updated;
  // otherwise we will try to start up a browser window with a pointer
  // to the old profile.
  // In a browser test, the application is not brought to the front, so
  // |lastProfile_| might be null.
  if (!_lastProfile || profilePath == _lastProfile->GetPath()) {
    // Force windowChangedToProfile: to set the lastProfile_ and also update the
    // relevant menuBridge objects.
    [self setLastProfile:nullptr];
    auto* profile_manager = g_browser_process->profile_manager();
    if (profile_manager) {
      // |profile_manager| is null in browser tests during shutdown.
      const base::FilePath last_used_path =
          profile_manager->GetLastUsedProfileDir(
              profile_manager->user_data_dir());
      Profile* last_used_profile =
          profile_manager->GetProfileByPath(last_used_path);
      if (last_used_profile)
        [self windowChangedToProfile:last_used_profile];
    }
  }

  _profileBookmarkMenuBridgeMap.erase(profilePath);
}

// Returns true if there is a modal window (either window- or application-
// modal) blocking the active browser. Note that tab modal dialogs (HTTP auth
// sheets) will not count as blocking the browser. But things like open/save
// dialogs that are window modal will block the browser.
- (BOOL)keyWindowIsModal {
  if ([NSApp modalWindow])
    return YES;

  Browser* browser = chrome::GetLastActiveBrowser();
  return browser && [[browser->window()->GetNativeWindow().GetNativeNSWindow()
                            attachedSheet] isKindOfClass:[NSWindow class]];
}

// Called to validate menu items when there are no key windows. All the
// items we care about have been set with the |commandDispatch:| action and
// a target of FirstResponder in IB. If it's not one of those, let it
// continue up the responder chain to be handled elsewhere. We pull out the
// tag as the cross-platform constant to differentiate and dispatch the
// various commands.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:) ||
      action == @selector(commandFromDock:)) {
    NSInteger tag = [item tag];
    if (_menuState &&  // NULL in tests.
        _menuState->SupportsCommand(tag)) {
      switch (tag) {
        // The File Menu commands are not automatically disabled by Cocoa when a
        // dialog sheet obscures the browser window, so we disable several of
        // them here.  We don't need to include IDC_CLOSE_WINDOW, because
        // app_controller is only activated when there are no key windows (see
        // function comment).
        case IDC_RESTORE_TAB:
          enable = ![self keyWindowIsModal] && [self canRestoreTab];
          break;
        // Browser-level items that open in new tabs should not open if there's
        // a window- or app-modal dialog.
        case IDC_OPEN_FILE:
        case IDC_NEW_TAB:
        case IDC_SHOW_HISTORY:
        case IDC_SHOW_BOOKMARK_MANAGER:
          enable = ![self keyWindowIsModal];
          break;
        // Browser-level items that open in new windows.
        case IDC_TASK_MANAGER:
          // Allow the user to open a new window if there's a window-modal
          // dialog.
          enable = ![self keyWindowIsModal];
          break;
        default:
          enable = _menuState->IsCommandEnabled(tag) ?
                   ![self keyWindowIsModal] : NO;
      }
    }

    // "Show as tab" should only appear when the current window is a popup.
    // Since |validateUserInterfaceItem:| is called only when there are no
    // key windows, we should just hide this.
    // This is handled outside of the switch statement because we want to hide
    // this regardless if the command is supported or not.
    if (tag == IDC_SHOW_AS_TAB) {
      NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item);
      [menuItem setHidden:YES];
    }
  } else if (action == @selector(terminate:)) {
    enable = YES;
  } else if (action == @selector(showPreferences:)) {
    enable = YES;
  } else if (action == @selector(orderFrontStandardAboutPanel:)) {
    enable = YES;
  } else if (action == @selector(commandFromDock:)) {
    enable = YES;
  } else if (action == @selector(toggleConfirmToQuit:)) {
    [self updateConfirmToQuitPrefMenuItem:static_cast<NSMenuItem*>(item)];
    enable = YES;
  } else if (action == @selector(executeApplication:)) {
    enable = YES;
  }
  return enable;
}

- (void)commandDispatch:(id)sender {
  Profile* lastProfile = [self safeLastProfileForNewWindows];

  // Handle the case where we're dispatching a command from a sender that's in a
  // browser window. This means that the command came from a background window
  // and is getting here because the foreground window is not a browser window.
  if ([sender respondsToSelector:@selector(window)]) {
    id delegate = [[sender window] windowController];
    if ([delegate respondsToSelector:@selector(commandDispatch:)]) {
      [delegate commandDispatch:sender];
      return;
    }
  }

  // Ignore commands during session restore's browser creation.  It uses a
  // nested run loop and commands dispatched during this operation cause
  // havoc.
  if (lastProfile && SessionRestore::IsRestoring(lastProfile) &&
      base::RunLoop::IsNestedOnCurrentThread()) {
    return;
  }

  // If not between -applicationDidFinishLaunching: and
  // -applicationWillTerminate:, ignore. This can happen when events are sitting
  // in the event queue while the browser is shutting down.
  if (!_keep_alive)
    return;

  NSInteger tag = [sender tag];

  // If there are no browser windows, and we are trying to open a browser
  // for a locked profile or the system profile or the guest profile but
  // guest mode is disabled, we have to show the User Manager instead as the
  // locked profile needs authentication and the system profile cannot have a
  // browser.
  const PrefService* prefService = g_browser_process->local_state();
  if (IsProfileSignedOut(lastProfile) || lastProfile->IsSystemProfile() ||
      (lastProfile->IsGuestSession() && prefService &&
       !prefService->GetBoolean(prefs::kBrowserGuestModeEnabled))) {
    ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileLocked);
    return;
  }

  switch (tag) {
    case IDC_NEW_TAB:
      // Create a new tab in an existing browser window (which we activate) if
      // possible.
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ExecuteCommand(browser, IDC_NEW_TAB);
        break;
      }
      FALLTHROUGH;  // To create new window.
    case IDC_NEW_WINDOW:
      CreateBrowser(lastProfile);
      break;
    case IDC_FOCUS_LOCATION:
      chrome::ExecuteCommand(ActivateOrCreateBrowser(lastProfile),
                             IDC_FOCUS_LOCATION);
      break;
    case IDC_FOCUS_SEARCH:
      chrome::ExecuteCommand(ActivateOrCreateBrowser(lastProfile),
                             IDC_FOCUS_SEARCH);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      CreateBrowser(lastProfile->GetPrimaryOTRProfile());
      break;
    case IDC_RESTORE_TAB:
      chrome::OpenWindowWithRestoredTabs(lastProfile);
      break;
    case IDC_OPEN_FILE:
      chrome::ExecuteCommand(CreateBrowser(lastProfile), IDC_OPEN_FILE);
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      // There may not be a browser open, so use the default profile.
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowClearBrowsingDataDialog(browser);
      } else {
        chrome::OpenClearBrowsingDataDialogWindow(lastProfile);
      }
      break;
    }
    case IDC_IMPORT_SETTINGS: {
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowImportDialog(browser);
      } else {
        chrome::OpenImportSettingsDialogWindow(lastProfile);
      }
      break;
    }
    case IDC_SHOW_BOOKMARK_MANAGER:
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowBookmarkManager(browser);
      } else {
        // No browser window, so create one for the bookmark manager tab.
        chrome::OpenBookmarkManagerWindow(lastProfile);
      }
      break;
    case IDC_SHOW_HISTORY:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowHistory(browser);
      else
        chrome::OpenHistoryWindow(lastProfile);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowDownloads(browser);
      else
        chrome::OpenDownloadsWindow(lastProfile);
      break;
    case IDC_MANAGE_EXTENSIONS:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowExtensions(browser, std::string());
      else
        chrome::OpenExtensionsWindow(lastProfile);
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowHelp(browser, chrome::HELP_SOURCE_MENU);
      else
        chrome::OpenHelpWindow(lastProfile, chrome::HELP_SOURCE_MENU);
      break;
    case IDC_TASK_MANAGER:
      chrome::OpenTaskManager(NULL);
      break;
    case IDC_OPTIONS:
      [self showPreferences:sender];
      break;
  }
}

// Run a (background) application in a new tab.
- (void)executeApplication:(id)sender {
  NSInteger tag = [sender tag];
  Profile* profile = [self lastProfile];
  DCHECK(profile);
  BackgroundApplicationListModel applications(profile);
  DCHECK(tag >= 0 &&
         tag < static_cast<int>(applications.size()));
  const extensions::Extension* extension = applications.GetExtension(tag);
  BackgroundModeManager::LaunchBackgroundApplication(profile, extension);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags. This will get called in the case where the
// frontmost window is not a browser window, and the user has command-clicked
// a button in a background browser window whose action is
// |-commandDispatchUsingKeyModifiers:|
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  DCHECK(sender);
  if ([sender respondsToSelector:@selector(window)]) {
    id delegate = [[sender window] windowController];
    if ([delegate respondsToSelector:
            @selector(commandDispatchUsingKeyModifiers:)]) {
      [delegate commandDispatchUsingKeyModifiers:sender];
    }
  }
}

// NSApplication delegate method called when someone clicks on the dock icon.
// To match standard mac behavior, we should open a new window if there are no
// browser windows.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)hasVisibleWindows {
  // If the browser is currently trying to quit, don't do anything and return NO
  // to prevent AppKit from doing anything.
  // TODO(rohitrao): Remove this code when http://crbug.com/40861 is resolved.
  if (browser_shutdown::IsTryingToQuit())
    return NO;

  // Bring all browser windows to the front. Specifically, this brings them in
  // front of any app windows. FocusWindowSet will also unminimize the most
  // recently minimized window if no windows in the set are visible.
  // If there are any, return here. Otherwise, the windows are panels or
  // notifications so we still need to open a new window.
  if (hasVisibleWindows) {
    std::set<gfx::NativeWindow> browserWindows;
    for (auto* browser : *BrowserList::GetInstance()) {
      // When focusing Chrome, don't focus any browser windows associated with
      // a currently running app shim, so ignore them.
      if (browser && browser->deprecated_is_app()) {
        extensions::ExtensionRegistry* registry =
            extensions::ExtensionRegistry::Get(browser->profile());
        const extensions::Extension* extension = registry->GetExtensionById(
            web_app::GetAppIdFromApplicationName(browser->app_name()),
            extensions::ExtensionRegistry::ENABLED);
        if (extension && extension->is_hosted_app())
          continue;
      }
      browserWindows.insert(browser->window()->GetNativeWindow());
    }
    if (!browserWindows.empty()) {
      NSWindow* keyWindow = [NSApp keyWindow];
      if (keyWindow && ![keyWindow isOnActiveSpace]) {
        // The key window is not on the active space. We must be mid-animation
        // for a space transition triggered by the dock. Delay the call to
        // |ui::FocusWindowSet| until the transition completes. Otherwise, the
        // wrong space's windows get raised, resulting in an off-screen key
        // window. It does not work to |ui::FocusWindowSet| twice, once here
        // and once in |activeSpaceDidChange:|, as that appears to break when
        // the omnibox is focused.
        //
        // This check relies on OS X setting the key window to a window on the
        // target space before calling this method.
        //
        // See http://crbug.com/309656.
        _reopenTime = base::TimeTicks::Now();
      } else {
        ui::FocusWindowSetOnCurrentSpace(browserWindows);
      }
      // Return NO; we've done (or soon will do) the deminiaturize, so
      // AppKit shouldn't do anything.
      return NO;
    }
  }

  // If launched as a hidden login item (due to installation of a persistent app
  // or by the user, for example in System Preferences->Accounts->Login Items),
  // allow session to be restored first time the user clicks on a Dock icon.
  // Normally, it'd just open a new empty page.
  {
    static BOOL doneOnce = NO;
    BOOL attemptRestore =
        apps::AppShimTerminationManager::Get()->ShouldRestoreSession() ||
        (!doneOnce && base::mac::WasLaunchedAsHiddenLoginItem());
    doneOnce = YES;
    if (attemptRestore) {
      Profile* lastProfile = [self lastProfile];
      if (!lastProfile) {
        // There is no session to be restored without a valid profile. Return NO
        // to do nothing.
        return NO;
      }
      SessionService* sessionService =
          SessionServiceFactory::GetForProfileForSessionRestore(lastProfile);
      if (sessionService &&
          sessionService->RestoreIfNecessary(std::vector<GURL>(),
                                             /* restore_apps */ false))
        return NO;
    }
  }

  // Otherwise open a new window.
  // If the last profile was locked, we have to open the User Manager, as the
  // profile requires authentication. Similarly, because guest mode and system
  // profile are implemented as forced incognito, we can't open a new guest
  // browser either, so we have to show the User Manager as well.
  Profile* lastProfile = [self lastProfile];
  if (!lastProfile) {
    // Without a profile there's nothing that can be done, but still return NO
    // to AppKit as there's nothing that it can do either.
    return NO;
  }
  if (lastProfile->IsGuestSession() || IsProfileSignedOut(lastProfile) ||
      lastProfile->IsSystemProfile()) {
    ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileLocked);
  } else if (ProfilePicker::ShouldShowAtLaunch()) {
    ProfilePicker::Show(
        ProfilePicker::EntryPoint::kNewSessionOnExistingProcess);
  } else {
    CreateBrowser(lastProfile);
  }

  // We've handled the reopen event, so return NO to tell AppKit not
  // to do anything.
  return NO;
}

- (void)initMenuState {
  _menuState = std::make_unique<CommandUpdaterImpl>(nullptr);
  _menuState->UpdateCommandEnabled(IDC_NEW_TAB, true);
  _menuState->UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  _menuState->UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW, true);
  _menuState->UpdateCommandEnabled(IDC_OPEN_FILE, true);
  _menuState->UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);
  _menuState->UpdateCommandEnabled(IDC_RESTORE_TAB, false);
  _menuState->UpdateCommandEnabled(IDC_FOCUS_LOCATION, true);
  _menuState->UpdateCommandEnabled(IDC_FOCUS_SEARCH, true);
  _menuState->UpdateCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER, true);
  _menuState->UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  _menuState->UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  _menuState->UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS, true);
  _menuState->UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  _menuState->UpdateCommandEnabled(IDC_IMPORT_SETTINGS, true);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  _menuState->UpdateCommandEnabled(IDC_FEEDBACK, true);
#endif
  _menuState->UpdateCommandEnabled(IDC_TASK_MANAGER, true);
}

// Conditionally adds the Profile menu to the main menu bar.
- (void)initProfileMenu {
  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenuItem* profileMenu = [mainMenu itemWithTag:IDC_PROFILE_MAIN_MENU];

  if (!profiles::IsMultipleProfilesEnabled()) {
    [mainMenu removeItem:profileMenu];
    return;
  }

  // The controller will unhide the menu if necessary.
  [profileMenu setHidden:YES];

  _profileMenuController.reset(
      [[ProfileMenuController alloc] initWithMainMenuItem:profileMenu]);
}

- (void)initShareMenu {
  _shareMenuController.reset([[ShareMenuController alloc] init]);
  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenu* fileMenu = [[mainMenu itemWithTag:IDC_FILE_MENU] submenu];
  NSString* shareMenuTitle = l10n_util::GetNSString(IDS_SHARE_MAC);
  NSMenuItem* shareMenuItem = [fileMenu itemWithTitle:shareMenuTitle];
  base::scoped_nsobject<NSMenu> shareSubmenu(
      [[NSMenu alloc] initWithTitle:shareMenuTitle]);
  [shareSubmenu setDelegate:_shareMenuController];
  [shareMenuItem setSubmenu:shareSubmenu];
}

// The Confirm to Quit preference is atypical in that the preference lives in
// the app menu right above the Quit menu item. This method will refresh the
// display of that item depending on the preference state.
- (void)updateConfirmToQuitPrefMenuItem:(NSMenuItem*)item {
  // Format the string so that the correct key equivalent is displayed.
  NSString* acceleratorString = [ConfirmQuitPanelController keyCommandString];
  NSString* title = l10n_util::GetNSStringF(IDS_CONFIRM_TO_QUIT_OPTION,
      base::SysNSStringToUTF16(acceleratorString));
  [item setTitle:title];

  const PrefService* prefService = g_browser_process->local_state();
  bool enabled = prefService->GetBoolean(prefs::kConfirmToQuitEnabled);
  [item setState:enabled ? NSOnState : NSOffState];
}

- (void)registerServicesMenuTypesTo:(NSApplication*)app {
  // Note that RenderWidgetHostViewCocoa implements NSServicesRequests which
  // handles requests from services.
  NSArray* types = @[ base::mac::CFToNSCast(kUTTypeUTF8PlainText) ];
  [app registerServicesMenuSendTypes:types returnTypes:types];
}

// Return null if Chrome is not ready or there is no ProfileManager.
- (Profile*)lastProfile {
  // Return the profile of the last-used Browser, if available.
  if (_lastProfile)
    return _lastProfile;

  if (![self isProfileReady])
    return nullptr;

  return GetLastProfileMac();
}

- (Profile*)safeLastProfileForNewWindows {
  Profile* profile = [self lastProfile];

  if (!profile)
    return nullptr;

  // Guest sessions must always be OffTheRecord. Use that when opening windows.
  if (profile->IsGuestSession())
    return profile->GetPrimaryOTRProfile();

  return profile;
}

// Returns true if a browser window may be opened for the last active profile.
- (bool)canOpenNewBrowser {
  Profile* profile = [self safeLastProfileForNewWindows];

  const PrefService* prefs = g_browser_process->local_state();
  return !profile->IsGuestSession() ||
         prefs->GetBoolean(prefs::kBrowserGuestModeEnabled);
}

// Various methods to open URLs that we get in a native fashion. We use
// StartupBrowserCreator here because on the other platforms, URLs to open come
// through the ProcessSingleton, and it calls StartupBrowserCreator. It's best
// to bottleneck the openings through that for uniform handling.
- (void)openUrls:(const std::vector<GURL>&)urls {
  if (!_startupComplete) {
    _startupUrls.insert(_startupUrls.end(), urls.begin(), urls.end());
    return;
  }
  // Pick the last used browser from a regular profile to open the urls.
  Profile* profile =
      g_browser_process->profile_manager()->GetLastUsedProfileAllowedByPolicy();
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = Browser::Create(
        Browser::CreateParams([self safeLastProfileForNewWindows], true));
    browser->window()->Show();
  }

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  launch.OpenURLsInBrowser(browser, false, urls);
}

- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply {
  NSString* urlStr = [[event paramDescriptorForKeyword:keyDirectObject]
                      stringValue];

  GURL gurl(base::SysNSStringToUTF8(urlStr));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  [self openUrlsReplacingNTP:gurlVector];
}

- (void)application:(NSApplication*)sender
          openFiles:(NSArray*)filenames {
  std::vector<GURL> gurlVector;
  for (NSString* file in filenames) {
    GURL gurl =
        net::FilePathToFileURL(base::FilePath([file fileSystemRepresentation]));
    gurlVector.push_back(gurl);
  }

  if (!gurlVector.empty())
    [self openUrlsReplacingNTP:gurlVector];
  else
    NOTREACHED() << "Nothing to open!";

  [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

// TODO(avi): When Chromium requires 10.13 as a minimum, remove the
// -[NSApplication application:openFiles:] override and the
// kInternetEventClass/kAEGetURL Apple Event registration in -mainMenuCreated.
- (void)application:(NSApplication*)sender openURLs:(NSArray<NSURL*>*)urls {
  std::vector<GURL> gurlVector;
  for (NSURL* url in urls)
    gurlVector.push_back(net::GURLWithNSURL(url));

  if (!gurlVector.empty())
    [self openUrlsReplacingNTP:gurlVector];
  else
    NOTREACHED() << "Nothing to open!";

  [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender {
  if (Browser* browser = ActivateBrowser([self lastProfile])) {
    // Show options tab in the active browser window.
    chrome::ShowSettings(browser);
  } else if ([self canOpenNewBrowser]) {
    // No browser window, so create one for the options tab.
    chrome::OpenOptionsWindow([self safeLastProfileForNewWindows]);
  } else {
    // No way to create a browser, default to the Profile Picker. On profile
    // selection, it opens the profile on the settings page.
    ProfilePicker::Show(ProfilePicker::EntryPoint::kUnableToCreateBrowser,
                        GURL(chrome::kChromeUISettingsURL));
  }
}

- (IBAction)orderFrontStandardAboutPanel:(id)sender {
  if (Browser* browser = ActivateBrowser([self lastProfile])) {
    chrome::ShowAboutChrome(browser);
  } else if ([self canOpenNewBrowser]) {
    // No browser window, so create one for the options tab.
    chrome::OpenAboutWindow([self safeLastProfileForNewWindows]);
  } else {
    // No way to create a browser, default to the User Manager. On profile
    // selection, it opens the profile on chrome help page.
    ProfilePicker::Show(ProfilePicker::EntryPoint::kUnableToCreateBrowser,
                        GURL(chrome::kChromeUIHelpURL));
  }
}

- (IBAction)toggleConfirmToQuit:(id)sender {
  PrefService* prefService = g_browser_process->local_state();
  bool enabled = prefService->GetBoolean(prefs::kConfirmToQuitEnabled);
  prefService->SetBoolean(prefs::kConfirmToQuitEnabled, !enabled);
}

// Explicitly bring to the foreground when creating new windows from the dock.
- (void)commandFromDock:(id)sender {
  [NSApp activateIgnoringOtherApps:YES];
  [self commandDispatch:sender];
}

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  NSMenu* dockMenu = [[[NSMenu alloc] initWithTitle: @""] autorelease];
  Profile* profile = [self lastProfile];

  BOOL profilesAdded = [_profileMenuController insertItemsIntoMenu:dockMenu
                                                          atOffset:0
                                                          fromDock:YES];
  if (profilesAdded)
    [dockMenu addItem:[NSMenuItem separatorItem]];

  NSString* titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_WINDOW_MAC);
  base::scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:titleStr
                                 action:@selector(commandFromDock:)
                          keyEquivalent:@""]);
  [item setTarget:self];
  [item setTag:IDC_NEW_WINDOW];
  [item setEnabled:[self validateUserInterfaceItem:item]];
  [dockMenu addItem:item];

  // |profile| can be NULL during unit tests.
  if (!profile ||
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
          IncognitoModePrefs::DISABLED) {
    titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_INCOGNITO_WINDOW_MAC);
    item.reset(
        [[NSMenuItem alloc] initWithTitle:titleStr
                                   action:@selector(commandFromDock:)
                            keyEquivalent:@""]);
    [item setTarget:self];
    [item setTag:IDC_NEW_INCOGNITO_WINDOW];
    [item setEnabled:[self validateUserInterfaceItem:item]];
    [dockMenu addItem:item];
  }

  // TODO(rickcam): Mock out BackgroundApplicationListModel, then add unit
  // tests which use the mock in place of the profile-initialized model.

  // Avoid breaking unit tests which have no profile.
  if (profile) {
    BackgroundApplicationListModel applications(profile);
    if (applications.size()) {
      int position = 0;
      NSString* menuStr =
          l10n_util::GetNSStringWithFixup(IDS_BACKGROUND_APPS_MAC);
      base::scoped_nsobject<NSMenu> appMenu(
          [[NSMenu alloc] initWithTitle:menuStr]);
      for (extensions::ExtensionList::const_iterator cursor =
               applications.begin();
           cursor != applications.end();
           ++cursor, ++position) {
        DCHECK_EQ(applications.GetPosition(cursor->get()), position);
        NSString* itemStr =
            base::SysUTF16ToNSString(base::UTF8ToUTF16((*cursor)->name()));
        base::scoped_nsobject<NSMenuItem> appItem(
            [[NSMenuItem alloc] initWithTitle:itemStr
                                       action:@selector(executeApplication:)
                                keyEquivalent:@""]);
        [appItem setTarget:self];
        [appItem setTag:position];
        [appMenu addItem:appItem];
      }
    }
  }

  return dockMenu;
}

- (const std::vector<GURL>&)startupUrls {
  return _startupUrls;
}

- (BookmarkMenuBridge*)bookmarkMenuBridge {
  return _bookmarkMenuBridge;
}

- (HistoryMenuBridge*)historyMenuBridge {
  return _historyMenuBridge.get();
}

- (TabMenuBridge*)tabMenuBridge {
  return _tabMenuBridge.get();
}

- (void)initAppShimMenuController {
  if (!_appShimMenuController)
    _appShimMenuController.reset([[AppShimMenuController alloc] init]);
}

- (void)setLastProfile:(Profile*)profile {
  if (profile == _lastProfile)
    return;

  if (profile == nullptr) {
    _lastProfile = nullptr;
    _lastProfileKeepAlive.reset();
    return;
  }

  _lastProfile = profile;
  _lastProfileKeepAlive = std::make_unique<ScopedProfileKeepAlive>(
      _lastProfile, ProfileKeepAliveOrigin::kAppControllerMac);
}

- (void)windowChangedToProfile:(Profile*)profile {
  if (_lastProfile == profile)
    return;

  // Before tearing down the menu controller bridges, return the history menu to
  // its initial state.
  if (_historyMenuBridge)
    _historyMenuBridge->ResetMenu();

  // Rebuild the menus with the new profile. The bookmarks submenu is cached to
  // avoid slowdowns when switching between profiles with large numbers of
  // bookmarks. Before caching, store whether it is hidden, make the menu item
  // visible, and restore its original hidden state after resetting the submenu.
  // This works around an apparent AppKit bug where setting a *different* NSMenu
  // submenu on a *hidden* menu item forces the item to become visible.
  // See https://crbug.com/497813 for more details.
  NSMenuItem* bookmarkItem = [[NSApp mainMenu] itemWithTag:IDC_BOOKMARKS_MENU];
  BOOL hidden = [bookmarkItem isHidden];
  [bookmarkItem setHidden:NO];
  [self setLastProfile:profile];

  auto& entry = _profileBookmarkMenuBridgeMap[profile->GetPath()];
  if (!entry) {
    // This creates a deep copy, but only the first 3 items in the root menu
    // are really wanted. This can probably be optimized, but lazy-loading of
    // the menu should reduce the impact in most flows.
    base::scoped_nsobject<NSMenu> submenu([[bookmarkItem submenu] copy]);
    [submenu setDelegate:nil];  // The delegate is also copied. Remove it.

    entry = std::make_unique<BookmarkMenuBridge>(profile, submenu);

    // Clear bookmarks from the old profile.
    entry->ClearBookmarkMenu();
  }
  _bookmarkMenuBridge = entry.get();

  // No need to |BuildMenu| here.  It is done lazily upon menu access.
  [bookmarkItem setSubmenu:_bookmarkMenuBridge->BookmarkMenu()];
  [bookmarkItem setHidden:hidden];

  _historyMenuBridge = std::make_unique<HistoryMenuBridge>(_lastProfile);
  _historyMenuBridge->BuildMenu();

  chrome::BrowserCommandController::
      UpdateSharedCommandsForIncognitoAvailability(
          _menuState.get(), _lastProfile);
  _profilePrefRegistrar = std::make_unique<PrefChangeRegistrar>();
  _profilePrefRegistrar->Init(_lastProfile->GetPrefs());
  _profilePrefRegistrar->Add(
      prefs::kIncognitoModeAvailability,
      base::BindRepeating(&chrome::BrowserCommandController::
                              UpdateSharedCommandsForIncognitoAvailability,
                          _menuState.get(), _lastProfile));
}

- (void)updateMenuItemKeyEquivalents {
  BOOL enableCloseTabShortcut = NO;
  id target = [NSApp targetForAction:@selector(performClose:)];

  // |target| is an instance of NSPopover or NSWindow.
  // If a popover (likely the dictionary lookup popover), we want Cmd-W to
  // close the popover so map it to "Close Window".
  // Otherwise, map Cmd-W to "Close Tab" if it's a browser window.
  if ([target isKindOfClass:[NSWindow class]]) {
    NSWindow* window = target;
    NSWindow* mainWindow = [NSApp mainWindow];
    if (!window || ([window parentWindow] == mainWindow)) {
      // If the target window is a child of the main window (e.g. a bubble), the
      // main window should be the one that handles the close menu item action.
      window = mainWindow;
    }
    Browser* browser = chrome::FindBrowserWithWindow(window);
    enableCloseTabShortcut = browser && browser->is_type_normal();
  }

  [self adjustCloseWindowMenuItemKeyEquivalent:enableCloseTabShortcut];
  [self adjustCloseTabMenuItemKeyEquivalent:enableCloseTabShortcut];
}

- (BOOL)application:(NSApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  return [userActivityType isEqualToString:NSUserActivityTypeBrowsingWeb];
}

- (BOOL)application:(NSApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:
          (void (^)(NSArray<id<NSUserActivityRestoring>>*))restorationHandler
{
  if (![userActivity.activityType
          isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    return NO;
  }

  NSString* originString = base::mac::ObjCCast<NSString>(
      [userActivity.userInfo objectForKey:handoff::kOriginKey]);
  handoff::Origin origin = handoff::OriginFromString(originString);
  UMA_HISTOGRAM_ENUMERATION(
      "OSX.Handoff.Origin", origin, handoff::ORIGIN_COUNT);

  NSURL* url = userActivity.webpageURL;
  if (!url)
    return NO;

  GURL gurl(base::SysNSStringToUTF8([url absoluteString]));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  [self openUrlsReplacingNTP:gurlVector];
  return YES;
}

- (void)application:(NSApplication*)application
    didFailToContinueUserActivityWithType:(NSString*)userActivityType
                                    error:(NSError*)error {
}

#pragma mark - Handoff Manager

- (void)passURLToHandoffManager:(const GURL&)handoffURL {
  [_handoffManager updateActiveURL:handoffURL];
}

- (void)updateHandoffManager:(content::WebContents*)webContents {
  if (!_handoffManager)
    _handoffManager.reset([[HandoffManager alloc] init]);

  GURL handoffURL = [self handoffURLFromWebContents:webContents];
  [self passURLToHandoffManager:handoffURL];
}

- (GURL)handoffURLFromWebContents:(content::WebContents*)webContents {
  if (!webContents)
    return GURL();

  Profile* profile =
      Profile::FromBrowserContext(webContents->GetBrowserContext());
  if (!profile)
    return GURL();

  // Handoff is not allowed from an incognito profile. To err on the safe side,
  // also disallow Handoff from a guest profile.
  if (!profile->IsRegularProfile())
    return GURL();

  if (!webContents)
    return GURL();

  return webContents->GetVisibleURL();
}

- (BOOL)isProfileReady {
  return !g_browser_process->browser_policy_connector()
              ->chrome_browser_cloud_management_controller()
              ->IsEnterpriseStartupDialogShowing();
}

#pragma mark - HandoffActiveURLObserverBridgeDelegate

- (void)handoffActiveURLChanged:(content::WebContents*)webContents {
  [self updateHandoffManager:webContents];
}

#pragma mark - ASWebAuthenticationSessionWebBrowserSessionHandling

// Note that both of these WebAuthenticationSession calls come in on a random
// worker thread, so it's important to hop to the main thread.

- (void)beginHandlingWebAuthenticationSessionRequest:
    (ASWebAuthenticationSessionRequest*)request API_AVAILABLE(macos(10.15)) {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    AuthSessionRequest::StartNewAuthSession(request, [self lastProfile]);
  });
}

- (void)cancelWebAuthenticationSessionRequest:
    (ASWebAuthenticationSessionRequest*)request API_AVAILABLE(macos(10.15)) {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    AuthSessionRequest::CancelAuthSession(request);
  });
}

@end  // @implementation AppController

//---------------------------------------------------------------------------

namespace {

void UpdateProfileInUse(Profile* profile, Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    AppController* controller =
        base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
    [controller windowChangedToProfile:profile];
  }
}

}  // namespace

namespace app_controller_mac {

bool IsOpeningNewWindow() {
  return g_is_opening_new_window;
}

void CreateGuestProfileIfNeeded() {
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      base::BindRepeating(&UpdateProfileInUse));
}

void EnterpriseStartupDialogClosed() {
  AppController* controller =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  if (controller != nil) {
    NSNotification* notify = [NSNotification
        notificationWithName:NSApplicationDidFinishLaunchingNotification
                      object:NSApp];
    [controller applicationDidFinishLaunching:notify];
  }
}

}  // namespace app_controller_mac
