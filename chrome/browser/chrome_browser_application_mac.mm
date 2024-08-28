// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_browser_application_mac.h"

#include <Carbon/Carbon.h>  // for <HIToolbox/Events.h>

#include "base/apple/call_with_eh_frame.h"
#include "base/check.h"
#include "base/command_line.h"
#import "base/mac/mac_util.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/mac/exception_processor.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/objc_zombie.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/native_event_processor_mac.h"
#include "content/public/browser/native_event_processor_observer_mac.h"
#include "content/public/common/content_features.h"
#include "ui/base/cocoa/accessibility_focus_overrider.h"

namespace chrome_browser_application_mac {

void RegisterBrowserCrApp() {
  [BrowserCrApplication sharedApplication];

  // If there was an invocation to NSApp prior to this method, then the NSApp
  // will not be a BrowserCrApplication, but will instead be an NSApplication.
  // This is undesirable and we must enforce that this doesn't happen.
  CHECK([NSApp isKindOfClass:[BrowserCrApplication class]]);
}

void InitializeHeadlessMode() {
  // In headless mode the browser window exists but is always hidden, so there
  // is no point in showing dock icon and menu bar.
  NSApp.activationPolicy = NSApplicationActivationPolicyAccessory;
}

void Terminate() {
  [NSApp terminate:nil];
}

void CancelTerminate() {
  [NSApp cancelTerminate:nil];
}

// A convenience function that activates `mode` if not already active in
// `state`.
void AddAccessibilityModeFlagsIfAbsent(
    content::BrowserAccessibilityState* state,
    ui::AXMode mode) {
  if (!state->GetAccessibilityMode().has_mode(mode.flags())) {
    state->AddAccessibilityModeFlags(mode);
  }
}

}  // namespace chrome_browser_application_mac

namespace {

// Calling -[NSEvent description] is rather slow to build up the event
// description. The description is stored in a crash key to aid debugging, so
// this helper function constructs a shorter, but still useful, description.
// See <https://crbug.com/770405>.
std::string DescriptionForNSEvent(NSEvent* event) {
  std::string desc = base::StringPrintf(
      "NSEvent type=%ld modifierFlags=0x%lx locationInWindow=(%g,%g)",
      event.type, event.modifierFlags, event.locationInWindow.x,
      event.locationInWindow.y);
  switch (event.type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp: {
      // Some NSEvents return a string with NUL in event.characters, see
      // <https://crbug.com/826908>. To make matters worse, in rare cases,
      // NSEvent.characters or NSEvent.charactersIgnoringModifiers can throw an
      // NSException complaining that "TSMProcessRawKeyCode failed". Since we're
      // trying to gather a crash key here, if that exception happens, just
      // remark that it happened and continue rather than crashing the browser.
      std::string characters, unmodified_characters;
      @try {
        characters = base::SysNSStringToUTF8([event.characters
            stringByReplacingOccurrencesOfString:@"\0"
                                      withString:@"\\x00"]);
        unmodified_characters =
            base::SysNSStringToUTF8([event.charactersIgnoringModifiers
                stringByReplacingOccurrencesOfString:@"\0"
                                          withString:@"\\x00"]);
      } @catch (id exception) {
        characters = "(exception)";
        unmodified_characters = "(exception)";
      }
      desc += base::StringPrintf(
          " keyCode=0x%d ARepeat=%d characters='%s' unmodifiedCharacters='%s'",
          event.keyCode, event.ARepeat, characters.c_str(),
          unmodified_characters.c_str());
      break;
    }
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeRightMouseUp:
      desc += base::StringPrintf(" buttonNumber=%ld clickCount=%ld",
                                 event.buttonNumber, event.clickCount);
      break;
    case NSEventTypeAppKitDefined:
    case NSEventTypeSystemDefined:
    case NSEventTypeApplicationDefined:
    case NSEventTypePeriodic:
      desc += base::StringPrintf(" subtype=%d data1=%ld data2=%ld",
                                 event.subtype, event.data1, event.data2);
      break;
    default:
      break;
  }
  return desc;
}

}  // namespace

@interface BrowserCrApplication () <NativeEventProcessor> {
  // A counter for enhanced user interface enable (+1) and disable (-1)
  // requests.
  int _AXEnhancedUserInterfaceRequests;
  BOOL _voiceOverEnabled;
  BOOL _sonomaAccessibilityRefinementsAreActive;
}

// Enables/disables screen reader support on changes to VoiceOver status.
- (void)voiceOverStateChanged:(BOOL)voiceOverEnabled;
@end

@implementation BrowserCrApplication {
  base::ObserverList<content::NativeEventProcessorObserver>::Unchecked
      _observers;
  BOOL _handlingSendEvent;
}

+ (void)initialize {
  if (self != [BrowserCrApplication class]) {
    return;
  }
  InstallObjcExceptionPreprocessor();

  cocoa_l10n_util::ApplyForcedRTL();
}

// Initialize NSApplication using the custom subclass.  Check whether NSApp
// was already initialized using another class, because that would break
// some things.
+ (NSApplication*)sharedApplication {
  NSApplication* app = [super sharedApplication];

  // +sharedApplication initializes the global NSApp, so if a specific
  // NSApplication subclass is requested, require that to be the one
  // delivered.  The practical effect is to require a consistent NSApp
  // across the executable.
  CHECK([NSApp isKindOfClass:self])
      << "NSApp must be of type " << [[self className] UTF8String]
      << ", not " << [[NSApp className] UTF8String];

  // If the message loop was initialized before NSApp is setup, the
  // message pump will be setup incorrectly.  Failing this implies
  // that RegisterBrowserCrApp() should be called earlier.
  CHECK(base::message_pump_apple::UsingCrApp())
      << "message_pump_apple::Create() is using the wrong pump implementation"
      << " for " << [[self className] UTF8String];

  return app;
}

- (void)finishLaunching {
  [super finishLaunching];

  // The accessibility feature, enabled from Finch, should already be
  // restricted to macOS 14, but we'll make an additional check here in the
  // code.
  _sonomaAccessibilityRefinementsAreActive =
      base::mac::MacOSVersion() >= 14'00'00 &&
      base::FeatureList::IsEnabled(
          features::kSonomaAccessibilityActivationRefinements);
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  // KVO of the system's VoiceOver state gets set up during initialization of
  // BrowserAccessibilityStateImplMac. The context is the browser's
  // global accessibility object, which we must check to ensure we're acting
  // on a notification we set up (vs. NSApplication, say).
  if (_sonomaAccessibilityRefinementsAreActive &&
      [keyPath isEqualToString:@"voiceOverEnabled"] &&
      context == content::BrowserAccessibilityState::GetInstance()) {
    NSNumber* newValueNumber = [change objectForKey:NSKeyValueChangeNewKey];

    // In the if statement below, we check newValueNumber's class before
    // accessing it to guard against crashes should the return type suddenly
    // change in the future. We DCHECK here to flag any such change.
    DCHECK([newValueNumber isKindOfClass:[NSNumber class]]);

    if ([newValueNumber isKindOfClass:[NSNumber class]]) {
      [self voiceOverStateChanged:[newValueNumber boolValue]];
    }

    return;
  }

  [super observeValueForKeyPath:keyPath
                       ofObject:object
                         change:change
                        context:context];
}

// AppKit menu customization overriding

- (void)_customizeFileMenuIfNeeded {
  // Whenever the main menu is set or modified, AppKit modifies it before using
  // it. AppKit calls -[NSApplication _customizeMainMenu], which calls out to a
  // number of customization methods, including -[NSApplication
  // _customizeFileMenuIfNeeded].
  //
  // -_customizeFileMenuIfNeeded does three things:
  //   1. it adds the "Close All" menu item as an alternate for "Close Window",
  //   2. for new-style document apps, it turns "Save" and "Save As..." into
  //      "Save..." and "Duplicate" respectively,
  //   3. depending on the "Close windows when quitting an application" system
  //      setting, it adds either "Quit and Keep Windows" or "Quit and Close All
  //      Windows" as an alternate for "Quit Chromium".
  //
  // While #1 is a nice-to-have, and #2 is irrelevant because Chromium isn't a
  // new-style document app, #3 is a problem. Chromium has its own session
  // management, and the menu item alternates that AppKit adds are making
  // promises that Chromium can't fulfill.
  //
  // Therefore, override this method to prevent AppKit from doing these menu
  // shenanigans. For #1, "Close All" is explicitly added to the File menu in
  // main_menu_builder.mm, and there is nothing lost by preventing the other
  // two.
  return;
}

////////////////////////////////////////////////////////////////////////////////
// HISTORICAL COMMENT (by viettrungluu, from
// http://codereview.chromium.org/1520006 with mild editing):
//
// A quick summary of the state of things (before the changes to shutdown):
//
// Currently, we are totally hosed (put in a bad state in which Cmd-W does the
// wrong thing, and which will probably eventually lead to a crash) if we begin
// quitting but termination is aborted for some reason.
//
// I currently know of two ways in which termination can be aborted:
// (1) Common case: a window has an onbeforeunload handler which pops up a
//     "leave web page" dialog, and the user answers "no, don't leave".
// (2) Uncommon case: popups are enabled (in Content Settings, i.e., the popup
//     blocker is disabled), and some nasty web page pops up a new window on
//     closure.
//
// I don't know of other ways in which termination can be aborted, but they may
// exist (or may be added in the future, for that matter).
//
// My CL [see above] does the following:
// a. Should prevent being put in a bad state (which breaks Cmd-W and leads to
//    crash) under all circumstances.
// b. Should completely handle (1) properly.
// c. Doesn't (yet) handle (2) properly and puts it in a weird state (but not
//    that bad).
// d. Any other ways of aborting termination would put it in that weird state.
//
// c. can be fixed by having the global flag reset on browser creation or
// similar (and doing so might also fix some possible d.'s as well). I haven't
// done this yet since I haven't thought about it carefully and since it's a
// corner case.
//
// The weird state: a state in which closing the last window quits the browser.
// This might be a bit annoying, but it's not dangerous in any way.
////////////////////////////////////////////////////////////////////////////////

// |-terminate:| is the entry point for orderly "quit" operations in Cocoa. This
// includes the application menu's quit menu item and keyboard equivalent, the
// application's dock icon menu's quit menu item, "quit" (not "force quit") in
// the Activity Monitor, and quits triggered by user logout and system restart
// and shutdown.
//
// The default |-terminate:| implementation ends the process by calling exit(),
// and thus never leaves the main run loop. This is unsuitable for Chrome since
// Chrome depends on leaving the main run loop to perform an orderly shutdown.
// We support the normal |-terminate:| interface by overriding the default
// implementation. Our implementation, which is very specific to the needs of
// Chrome, works by asking the application delegate to terminate using its
// |-tryToTerminateApplication:| method.
//
// |-tryToTerminateApplication:| differs from the standard
// |-applicationShouldTerminate:| in that no special event loop is run in the
// case that immediate termination is not possible (e.g., if dialog boxes
// allowing the user to cancel have to be shown). Instead, this method sets a
// flag and tries to close all browsers. This flag causes the closure of the
// final browser window to begin actual tear-down of the application.
// Termination is cancelled by resetting this flag. The standard
// |-applicationShouldTerminate:| is not supported, and code paths leading to it
// must be redirected.
//
// When the last browser has been destroyed, the BrowserList calls
// chrome::OnAppExiting(), which is the point of no return. That will cause
// the NSApplicationWillTerminateNotification to be posted, which ends the
// NSApplication event loop, so final post- MessageLoop::Run() work is done
// before exiting.
- (void)terminate:(id)sender {
  [AppController.sharedController tryToTerminateApplication:self];
  // Return, don't exit. The application is responsible for exiting on its own.
}

- (void)cancelTerminate:(id)sender {
  [AppController.sharedController stopTryingToTerminateApplication:self];
}

- (NSEvent*)nextEventMatchingMask:(NSEventMask)mask
                        untilDate:(NSDate*)expiration
                           inMode:(NSString*)mode
                          dequeue:(BOOL)dequeue {
  __block NSEvent* event = nil;
  base::apple::CallWithEHFrame(^{
    event = [super nextEventMatchingMask:mask
                               untilDate:expiration
                                  inMode:mode
                                 dequeue:dequeue];
  });
  return event;
}

- (BOOL)sendAction:(SEL)anAction to:(id)aTarget from:(id)sender {
  // The Dock menu contains an automagic section where you can select
  // amongst open windows.  If a window is closed via JavaScript while
  // the menu is up, the menu item for that window continues to exist.
  // When a window is selected this method is called with the
  // now-freed window as |aTarget|.  Short-circuit the call if
  // |aTarget| is not a valid window.
  if (anAction == @selector(_selectWindow:)) {
    // Not using -[NSArray containsObject:] because |aTarget| may be a
    // freed object.
    BOOL found = NO;
    for (NSWindow* window in [self windows]) {
      if (window == aTarget) {
        found = YES;
        break;
      }
    }
    if (!found) {
      return NO;
    }
  }

  // When a Cocoa control is wired to a freed object, we get crashers
  // in the call to |super| with no useful information in the
  // backtrace.  Attempt to add some useful information.

  // If the action is something generic like -commandDispatch:, then
  // the tag is essential.
  NSInteger tag = 0;
  if ([sender isKindOfClass:[NSControl class]]) {
    tag = [sender tag];
    if (tag == 0 || tag == -1) {
      tag = [sender selectedTag];
    }
  } else if ([sender isKindOfClass:[NSMenuItem class]]) {
    tag = [sender tag];
  }

  NSString* actionString = NSStringFromSelector(anAction);
  std::string value = base::StringPrintf("%s tag %ld sending %s to %p",
      [[sender className] UTF8String],
      static_cast<long>(tag),
      [actionString UTF8String],
      aTarget);

  static crash_reporter::CrashKeyString<256> sendActionKey("sendaction");
  crash_reporter::ScopedCrashKeyString scopedKey(&sendActionKey, value);

  __block BOOL rv;
  base::apple::CallWithEHFrame(^{
    rv = [super sendAction:anAction to:aTarget from:sender];
  });
  return rv;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  TRACE_EVENT0("toplevel", "BrowserCrApplication::sendEvent");

  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT_INSTANT1("toplevel", "KeyWindow", TRACE_EVENT_SCOPE_THREAD,
                       "KeyWin", [[NSApp keyWindow] windowNumber]);

  static crash_reporter::CrashKeyString<256> nseventKey("nsevent");
  crash_reporter::ScopedCrashKeyString scopedKey(&nseventKey,
                                                 DescriptionForNSEvent(event));

  base::apple::CallWithEHFrame(^{
    static const bool kKioskMode =
        base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
    if (kKioskMode) {
      // In kiosk mode, we want to prevent context menus from appearing,
      // so simply discard menu-generating events instead of passing them
      // along.
      BOOL couldTriggerContextMenu =
          event.type == NSEventTypeRightMouseDown ||
          (event.type == NSEventTypeLeftMouseDown &&
           (event.modifierFlags & NSEventModifierFlagControl));
      if (couldTriggerContextMenu)
        return;
    }
    base::mac::ScopedSendingEvent sendingEventScoper;
    content::ScopedNotifyNativeEventProcessorObserver scopedObserverNotifier(
        &self->_observers, event);
    // Mac Eisu and Kana keydown events are by default swallowed by sendEvent
    // and sent directly to IME, which prevents ui keydown events from firing.
    // These events need to be sent to [NSApp keyWindow] for handling.
    if (event.type == NSEventTypeKeyDown &&
        (event.keyCode == kVK_JIS_Eisu || event.keyCode == kVK_JIS_Kana)) {
      [NSApp.keyWindow sendEvent:event];
    } else {
      [super sendEvent:event];
    }
  });
}

// Accessibility Support

- (void)enableScreenReaderCompleteMode:(BOOL)enable {
  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();

  if (enable) {
    accessibility_state->OnScreenReaderDetected();
  } else {
    accessibility_state->OnScreenReaderStopped();
  }
}

// We need to call enableScreenReaderCompleteMode:YES from performSelector:...
// but there's no way to supply a BOOL as a parameter, so we have this
// explicit enable... helper method.
- (void)enableScreenReaderCompleteMode {
  _AXEnhancedUserInterfaceRequests = 0;
  [self enableScreenReaderCompleteMode:YES];
}

- (void)voiceOverStateChanged:(BOOL)voiceOverEnabled {
  _voiceOverEnabled = voiceOverEnabled;

  [self enableScreenReaderCompleteMode:voiceOverEnabled];
}

- (BOOL)voiceOverStateForTesting {
  return _voiceOverEnabled;
}

// Enables or disables screen reader support for non-VoiceOver assistive
// technology (AT), possibly after a delay.
//
// Now that we directly monitor VoiceOver status, we no longer watch for
// changes to AXEnhancedUserInterface for that signal from VO. However, other
// AT can set a value for AXEnhancedUserInterface, so we can't ignore it.
// Unfortunately, as of macOS Sonoma, we sometimes see spurious changes to
// AXEnhancedUserInterface (quick on and off). We debounce by waiting for these
// changes to settle down before updating the screen reader state.
- (void)enableScreenReaderCompleteModeAfterDelay:(BOOL)enable {
  // If VoiceOver is already explicitly enabled, ignore requests from other AT.
  if (_voiceOverEnabled) {
    return;
  }

  // If this is a request to disable screen reader support, and we haven't seen
  // a corresponding enable request, go ahead and disable.
  if (!enable && _AXEnhancedUserInterfaceRequests == 0) {
    [self enableScreenReaderCompleteMode:NO];
    return;
  }

  // Use a counter to track requests for changes to the screen reader state.
  if (enable) {
    _AXEnhancedUserInterfaceRequests++;
  } else {
    _AXEnhancedUserInterfaceRequests--;
  }

  DCHECK(_AXEnhancedUserInterfaceRequests >= 0);

  // _AXEnhancedUserInterfaceRequests > 0 means we want to enable screen
  // reader support, but we'll delay that action until there are no more state
  // change requests within a two-second window. Cancel any pending
  // performSelector:..., and schedule a new one to restart the countdown.
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (enableScreenReaderCompleteMode)
                                             object:nil];

  if (_AXEnhancedUserInterfaceRequests > 0) {
    const float kTwoSecondDelay = 2.0;
    [self performSelector:@selector(enableScreenReaderCompleteMode)
               withObject:nil
               afterDelay:kTwoSecondDelay];
  }
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  // This is an undocumented attribute that's set when VoiceOver is turned
  // on/off.
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"]) {
    if (_sonomaAccessibilityRefinementsAreActive) {
      // We no longer rely on this signal for VoiceOver state changes, but we
      // pay attention to it in case other applications use it to request
      // accessibility activation.
      [self enableScreenReaderCompleteModeAfterDelay:[value boolValue]];
    } else {
      content::BrowserAccessibilityState* accessibility_state =
          content::BrowserAccessibilityState::GetInstance();
      if ([value boolValue]) {
        accessibility_state->OnScreenReaderDetected();
      } else {
        accessibility_state->OnScreenReaderStopped();
      }
    }
  }
  return [super accessibilitySetValue:value forAttribute:attribute];
}

- (id)accessibilityFocusedUIElement {
  if (id forced_focus = ui::AccessibilityFocusOverrider::GetFocusedUIElement())
    return forced_focus;
  return [super accessibilityFocusedUIElement];
}

- (NSAccessibilityRole)accessibilityRole {
  // For non-VoiceOver assistive technology (AT), such as Voice Control, Apple
  // recommends turning on a11y when an AT accesses the 'accessibilityRole'
  // property. This function is accessed frequently, so we only change the
  // accessibility state when accessibility is already disabled.
  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();

  if (_sonomaAccessibilityRefinementsAreActive) {
    if (!_voiceOverEnabled) {
      chrome_browser_application_mac::AddAccessibilityModeFlagsIfAbsent(
          accessibility_state, ui::AXMode::kNativeAPIs);
    }
  } else {
    if (!accessibility_state->GetAccessibilityMode().has_mode(
            ui::kAXModeBasic.flags())) {
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeBasic);
    }
  }

  return [super accessibilityRole];
}

- (void)addNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  _observers.AddObserver(observer);
}

- (void)removeNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  _observers.RemoveObserver(observer);
}

@end
