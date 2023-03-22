// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"

#if defined(__OBJC__)

#import <AuthenticationServices/AuthenticationServices.h>
#import <Cocoa/Cocoa.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/prefs/pref_change_registrar.h"

class AppControllerProfileObserver;
class AppControllerNativeThemeObserver;
@class AppShimMenuController;
class BookmarkMenuBridge;
class CommandUpdater;
class GURL;
@class HandoffManager;
class HistoryMenuBridge;
class HandoffObserver;
class Profile;
@class ProfileMenuController;
class QuitWithAppsController;
class ScopedKeepAlive;
@class ShareMenuController;
class TabMenuBridge;

namespace ui {
class ColorProvider;
}  // namespace ui

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController
    : NSObject <NSUserInterfaceValidations,
                NSMenuDelegate,
                NSApplicationDelegate,
                ASWebAuthenticationSessionWebBrowserSessionHandling> {
 @private
  // Manages the state of the command menu items.
  std::unique_ptr<CommandUpdater> _menuState;

  // The profile last used by a Browser. It is this profile that was used to
  // build the user-data specific main menu items.
  raw_ptr<Profile> _lastProfile;

  // The ProfileObserver observes the ProfileAttrbutesStorage and gets notified
  // when a profile has been deleted.
  std::unique_ptr<AppControllerProfileObserver>
      _profileAttributesStorageObserver;

  // The NativeThemeObserver observes system-wide theme related settings
  // change.
  std::unique_ptr<AppControllerNativeThemeObserver> _nativeThemeObserver;

  // Management of the bookmark menu which spans across all windows
  // (and Browser*s). |profileBookmarkMenuBridgeMap_| is a cache that owns one
  // pointer to a BookmarkMenuBridge for each profile. |bookmarkMenuBridge_| is
  // a weak pointer that is updated to match the corresponding cache entry
  // during a profile switch.
  raw_ptr<BookmarkMenuBridge> _bookmarkMenuBridge;
  std::map<base::FilePath, std::unique_ptr<BookmarkMenuBridge>>
      _profileBookmarkMenuBridgeMap;

  std::unique_ptr<HistoryMenuBridge> _historyMenuBridge;

  // Controller that manages main menu items for packaged apps.
  base::scoped_nsobject<AppShimMenuController> _appShimMenuController;

  // The profile menu, which appears right before the Help menu. It is only
  // available when multiple profiles is enabled.
  base::scoped_nsobject<ProfileMenuController> _profileMenuController;

  // Controller for the macOS system share menu.
  base::scoped_nsobject<ShareMenuController> _shareMenuController;

  std::unique_ptr<TabMenuBridge> _tabMenuBridge;

  // If we're told to open URLs (in particular, via |-application:openURLs:| by
  // Launch Services) before we've launched the browser, we queue them up in
  // |startupUrls_| so that they can go in the first browser window/tab.
  std::vector<GURL> _startupUrls;
  BOOL _startupComplete;

  // Outlets for the close tab/window menu items so that we can adjust the
  // commmand-key equivalent depending on the kind of window and how many
  // tabs it has.
  NSMenuItem* _closeTabMenuItem;
  NSMenuItem* _closeWindowMenuItem;

  std::unique_ptr<PrefChangeRegistrar> _profilePrefRegistrar;
  PrefChangeRegistrar _localPrefRegistrar;

  // Displays a notification when quitting while apps are running.
  scoped_refptr<QuitWithAppsController> _quitWithAppsController;

  // Responsible for maintaining all state related to the Handoff feature.
  base::scoped_nsobject<HandoffManager> _handoffManager;

  // Observes changes to the active web contents.
  std::unique_ptr<HandoffObserver> _handoff_observer;

  // This will be true after receiving a NSWorkspaceWillPowerOffNotification.
  BOOL _isPoweringOff;

  // This will be true after receiving a |-applicationWillTerminate:| event.
  BOOL _isShuttingDown;

  // Request to keep the browser alive during that object's lifetime.
  std::unique_ptr<ScopedKeepAlive> _keep_alive;

  // Remembers whether _lastProfile had TabRestoreService entries. This is saved
  // when _lastProfile is destroyed and Chromium enters the zero-profile state.
  //
  // By remembering this bit, Chromium knows whether to enable or disable
  // Cmd+Shift+T and the related "File > Reopen Closed Tab" entry.
  BOOL _tabRestoreWasEnabled;

  // The color provider associated with the last active browser view.
  raw_ptr<const ui::ColorProvider, DanglingUntriaged> _lastActiveColorProvider;
}

@property(readonly, nonatomic) BOOL startupComplete;
@property(readonly, nonatomic) Profile* lastProfileIfLoaded;

// DEPRECATED: use lastProfileIfLoaded instead.
// TODO(https://crbug.com/1176734): May be blocking, migrate all callers to
// |-lastProfileIfLoaded|.
@property(readonly, nonatomic) Profile* lastProfile;

// This method is called very early in application startup after the main menu
// has been created.
- (void)mainMenuCreated;

- (void)didEndMainMessageLoop;

// Try to close all browser windows, and if that succeeds then quit.
- (BOOL)tryToTerminateApplication:(NSApplication*)app;

// Stop trying to terminate the application. That is, prevent the final browser
// window closure from causing the application to quit.
- (void)stopTryingToTerminateApplication:(NSApplication*)app;

// Run the quit confirmation panel and return whether or not to continue
// quitting.
- (BOOL)runConfirmQuitPanel;

// Indicate that the system is powering off or logging out.
- (void)willPowerOff:(NSNotification*)inNotification;

// Returns true if there is a modal window (either window- or application-
// modal) blocking the active browser. Note that tab modal dialogs (HTTP auth
// sheets) will not count as blocking the browser. But things like open/save
// dialogs that are window modal will block the browser.
- (BOOL)keyWindowIsModal;

// Called when the user picks a menu item when there are no key windows, or when
// there is no foreground browser window. Calls through to the browser object to
// execute the command. This assumes that the command is supported and doesn't
// check, otherwise it should have been disabled in the UI in
// |-validateUserInterfaceItem:|.
- (void)commandDispatch:(id)sender;

// Helper function called by -commandDispatch:, to actually execute the command.
// This runs after -commandDispatch: has obtained a pointer to the last Profile
// (which possibly requires an async Profile load).
- (void)executeCommand:(id)sender withProfile:(Profile*)profile;

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender;
- (IBAction)showPreferencesForProfile:(Profile*)profile;

// Redirect in the menu item from the expected target of "File's
// Owner" (NSApplication) for a Branded About Box
- (IBAction)orderFrontStandardAboutPanel:(id)sender;
- (IBAction)orderFrontStandardAboutPanelForProfile:(Profile*)profile;

// Toggles the "Confirm to Quit" preference.
- (IBAction)toggleConfirmToQuit:(id)sender;

// Delegate method to return the dock menu.
- (NSMenu*)applicationDockMenu:(NSApplication*)sender;

// Get the URLs that Launch Services expects the browser to open at startup.
- (const std::vector<GURL>&)startupUrls;

- (BookmarkMenuBridge*)bookmarkMenuBridge;
- (HistoryMenuBridge*)historyMenuBridge;
- (TabMenuBridge*)tabMenuBridge;

// Initializes the AppShimMenuController. This enables changing the menu bar for
// apps.
- (void)initAppShimMenuController;

// Called when the user has changed browser windows, meaning the backing profile
// may have changed. This can cause a rebuild of the user-data menus. This is a
// no-op if the new profile is the same as the current one. This can be either
// the original or the incognito profile.
- (void)setLastProfile:(Profile*)profile;

// Returns the last active ColorProvider.
- (const ui::ColorProvider&)lastActiveColorProvider;

// This is called when the system wide light or dark mode changes.
- (void)nativeThemeDidChange;

// Certain NSMenuItems [Close Tab and Close Window] have different
// keyEquivalents depending on context. This must be invoked in two locations:
//   * In menuNeedsUpdate:, which is called prior to showing the NSMenu.
//   * In CommandDispatcher, which independently searches for a matching
//     keyEquivalent.
- (void)updateMenuItemKeyEquivalents;

// Returns YES if `window` is a normal, tabbed, non-app browser window.
// Serves as a swizzle point for unit tests to avoid creating Browser
// instances.
- (BOOL)windowHasBrowserTabs:(NSWindow*)window;

// Testing API.
- (void)setCloseWindowMenuItemForTesting:(NSMenuItem*)menuItem;
- (void)setCloseTabMenuItemForTesting:(NSMenuItem*)menuItem;
- (void)setLastProfileForTesting:(Profile*)profile;

@end

#endif  // __OBJC__

// Functions that may be accessed from non-Objective-C C/C++ code.

namespace app_controller_mac {

// True if we are currently handling an IDC_NEW_{TAB,WINDOW} command. Used in
// SessionService::Observe() to get around windows/linux and mac having
// different models of application lifetime.
bool IsOpeningNewWindow();

// Create a guest profile if one is needed. Afterwards, even if the profile
// already existed, notify the AppController of the profile in use.
void CreateGuestProfileIfNeeded();

// Called when Enterprise startup dialog is close and repost
// applicationDidFinished notification.
void EnterpriseStartupDialogClosed();

// Tells RunInSafeProfile() or RunInSpecificSafeProfile() what to do if the
// profile cannot be loaded from disk.
enum ProfileLoadFailureBehavior {
  // Silently fail, and run |callback| with nullptr.
  kIgnoreOnFailure,
  // Show the profile picker, and run |callback| with nullptr.
  kShowProfilePickerOnFailure,
};

// Tries to load the profile returned by |-safeProfileForNewWindows:|. If it
// succeeds, calls |callback| with it.
//
// |callback| must be valid.
void RunInLastProfileSafely(base::OnceCallback<void(Profile*)> callback,
                            ProfileLoadFailureBehavior on_failure);

// Tries to load the profile in |profile_dir|. If it succeeds, calls
// |callback| with it. If the profile was already loaded, |callback| runs
// immediately.
//
// |callback| must be valid.
void RunInProfileSafely(const base::FilePath& profile_dir,
                        base::OnceCallback<void(Profile*)> callback,
                        ProfileLoadFailureBehavior on_failure);

// Waits for the TabRestoreService to have loaded its entries, then calls
// OpenWindowWithRestoredTabs().
//
// Owned by itself.
class TabRestorer : public sessions::TabRestoreServiceObserver {
 public:
  // Restore the most recent tab in |profile|, e.g. for Cmd+Shift+T.
  static void RestoreMostRecent(Profile* profile);

  // Restore a specific tab in |profile|, e.g. for a History menu item.
  // |session_id| can be a |TabRestoreService::Entry::id|, or a
  // |TabRestoreEntryService::Entry::original_id|.
  static void RestoreByID(Profile* profile, SessionID session_id);

  ~TabRestorer() override;

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

 private:
  TabRestorer(Profile* profile, SessionID session_id);

  // Performs the tab restore. Called either in TabRestoreServiceLoaded(), or
  // directly from RestoreMostRecent()/RestoreByID() if the service was already
  // loaded.
  static void DoRestoreTab(Profile* profile, SessionID session_id);

  base::ScopedObservation<sessions::TabRestoreService,
                          sessions::TabRestoreServiceObserver>
      observation_{this};
  raw_ptr<Profile> profile_;
  SessionID session_id_;
};

}  // namespace app_controller_mac

#endif  // CHROME_BROWSER_APP_CONTROLLER_MAC_H_
