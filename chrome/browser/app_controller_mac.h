// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_

#if defined(__OBJC__)

#import <Cocoa/Cocoa.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"

class AppControllerProfileObserver;
@class AppShimMenuController;
class BookmarkMenuBridge;
class CommandUpdater;
class GURL;
class HandoffActiveURLObserverBridge;
@class HandoffManager;
class HistoryMenuBridge;
class Profile;
@class ProfileMenuController;
class QuitWithAppsController;
class ScopedKeepAlive;
@class ShareMenuController;
class TabMenuBridge;

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController : NSObject<NSUserInterfaceValidations,
                                    NSMenuDelegate,
                                    NSApplicationDelegate> {
 @private
  // Manages the state of the command menu items.
  std::unique_ptr<CommandUpdater> menuState_;

  // The profile last used by a Browser. It is this profile that was used to
  // build the user-data specific main menu items.
  Profile* lastProfile_;

  // The ProfileObserver observes the ProfileAttrbutesStorage and gets notified
  // when a profile has been deleted.
  std::unique_ptr<AppControllerProfileObserver>
      profileAttributesStorageObserver_;

  // Management of the bookmark menu which spans across all windows
  // (and Browser*s). |profileBookmarkMenuBridgeMap_| is a cache that owns one
  // pointer to a BookmarkMenuBridge for each profile. |bookmarkMenuBridge_| is
  // a weak pointer that is updated to match the corresponding cache entry
  // during a profile switch.
  BookmarkMenuBridge* bookmarkMenuBridge_;
  std::map<base::FilePath, std::unique_ptr<BookmarkMenuBridge>>
      profileBookmarkMenuBridgeMap_;

  std::unique_ptr<HistoryMenuBridge> historyMenuBridge_;

  // Controller that manages main menu items for packaged apps.
  base::scoped_nsobject<AppShimMenuController> appShimMenuController_;

  // The profile menu, which appears right before the Help menu. It is only
  // available when multiple profiles is enabled.
  base::scoped_nsobject<ProfileMenuController> profileMenuController_;

  // Controller for the macOS system share menu.
  base::scoped_nsobject<ShareMenuController> shareMenuController_;

  std::unique_ptr<TabMenuBridge> tabMenuBridge_;

  // If we're told to open URLs (in particular, via |-application:openFiles:| by
  // Launch Services) before we've launched the browser, we queue them up in
  // |startupUrls_| so that they can go in the first browser window/tab.
  std::vector<GURL> startupUrls_;
  BOOL startupComplete_;

  // Outlets for the close tab/window menu items so that we can adjust the
  // commmand-key equivalent depending on the kind of window and how many
  // tabs it has.
  NSMenuItem* closeTabMenuItem_;
  NSMenuItem* closeWindowMenuItem_;

  // If we are expecting a workspace change in response to a reopen
  // event, the time we got the event. A null time otherwise.
  base::TimeTicks reopenTime_;

  std::unique_ptr<PrefChangeRegistrar> profilePrefRegistrar_;
  PrefChangeRegistrar localPrefRegistrar_;

  // Displays a notification when quitting while apps are running.
  scoped_refptr<QuitWithAppsController> quitWithAppsController_;

  // Responsible for maintaining all state related to the Handoff feature.
  base::scoped_nsobject<HandoffManager> handoffManager_;

  // Observes changes to the active URL.
  std::unique_ptr<HandoffActiveURLObserverBridge>
      handoff_active_url_observer_bridge_;

  // This will be true after receiving a NSWorkspaceWillPowerOffNotification.
  BOOL isPoweringOff_;

  // Request to keep the browser alive during that object's lifetime.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
}

@property(readonly, nonatomic) BOOL startupComplete;
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

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender;

// Redirect in the menu item from the expected target of "File's
// Owner" (NSApplication) for a Branded About Box
- (IBAction)orderFrontStandardAboutPanel:(id)sender;

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
// no-op if the new profile is the same as the current one. This will always be
// the original profile and never incognito.
- (void)windowChangedToProfile:(Profile*)profile;

// Certain NSMenuItems [Close Tab and Close Window] have different
// keyEquivalents depending on context. This must be invoked in two locations:
//   * In menuNeedsUpdate:, which is called prior to showing the NSMenu.
//   * In CommandDispatcher, which independently searches for a matching
//     keyEquivalent.
- (void)updateMenuItemKeyEquivalents;

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

}  // namespace app_controller_mac

#endif  // CHROME_BROWSER_APP_CONTROLLER_MAC_H_
