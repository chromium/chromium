// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

// API function that enables or disables web content accessibility support.
class AccessibilityPrivateSetNativeAccessibilityEnabledFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetNativeAccessibilityEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeAccessibilityEnabled",
      ACCESSIBILITY_PRIVATE_SETNATIVEACCESSIBILITYENABLED)
};

// API function that sets the location of the accessibility focus ring.
class AccessibilityPrivateSetFocusRingsFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetFocusRingsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setFocusRings",
                             ACCESSIBILITY_PRIVATE_SETFOCUSRING)
};

// API function that sets the location of the accessibility highlights.
class AccessibilityPrivateSetHighlightsFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetHighlightsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setHighlights",
                             ACCESSIBILITY_PRIVATE_SETHIGHLIGHTS)
};

// API function that sets keyboard capture mode.
class AccessibilityPrivateSetKeyboardListenerFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetKeyboardListenerFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setKeyboardListener",
                             ACCESSIBILITY_PRIVATE_SETKEYBOARDLISTENER)
};

// API function that darkens or undarkens the screen.
class AccessibilityPrivateDarkenScreenFunction : public ExtensionFunction {
  ~AccessibilityPrivateDarkenScreenFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.darkenScreen",
                             ACCESSIBILITY_PRIVATE_DARKENSCREEN)
};

// Opens a specified subpage in Chrome settings.
class AccessibilityPrivateOpenSettingsSubpageFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateOpenSettingsSubpageFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.openSettingsSubpage",
                             ACCESSIBILITY_PRIVATE_OPENSETTINGSSUBPAGE)
};

#if defined(OS_CHROMEOS)
// API function that sets native ChromeVox ARC support.
class AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction()
      override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp",
      ACCESSIBILITY_PRIVATE_SETNATIVECHROMEVOXARCSUPPORTFORCURRENTAPP)
};

// API function that injects key events.
class AccessibilityPrivateSendSyntheticKeyEventFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendSyntheticKeyEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticKeyEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICKEYEVENT)
};

// API function that enables or disables mouse events in ChromeVox.
class AccessibilityPrivateEnableChromeVoxMouseEventsFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateEnableChromeVoxMouseEventsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.enableChromeVoxMouseEvents",
                             ACCESSIBILITY_PRIVATE_ENABLECHROMEVOXMOUSEEVENTS)
};

// API function that injects mouse events.
class AccessibilityPrivateSendSyntheticMouseEventFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendSyntheticMouseEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticMouseEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICMOUSEEVENT)
};

// API function that is called when the Select-to-Speak extension state changes.
class AccessibilityPrivateOnSelectToSpeakStateChangedFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateOnSelectToSpeakStateChangedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.onSelectToSpeakStateChanged",
                             ACCESSIBILITY_PRIVATE_ONSELECTTOSPEAKSTATECHANGED)
};

// API function that is called when the Autoclick extension finds scrollable
// bounds.
class AccessibilityPrivateOnScrollableBoundsForPointFoundFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateOnScrollableBoundsForPointFoundFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.onScrollableBoundsForPointFound",
      ACCESSIBILITY_PRIVATE_ONSCROLLABLEBOUNDSFORPOINTFOUND)
};

// API function that is called when a user toggles Dictation from another
// acessibility feature.
class AccessibilityPrivateToggleDictationFunction : public ExtensionFunction {
  ~AccessibilityPrivateToggleDictationFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.toggleDictation",
                             ACCESSIBILITY_PRIVATE_TOGGLEDICTATION)
};

// API function that is called to show or hide the Switch Access menu.
class AccessibilityPrivateSetSwitchAccessMenuStateFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetSwitchAccessMenuStateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setSwitchAccessMenuState",
                             ACCESSIBILITY_PRIVATE_SETSWITCHACCESSMENUSTATE)
};

// API function that requests that key events be forwarded to the Switch
// Access extension.
class AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.forwardKeyEventsToSwitchAccess",
      ACCESSIBILITY_PRIVATE_FORWARDKEYEVENTSTOSWITCHACCESS)
};

// API function that is called to get the device's battery status as a string.
class AccessibilityPrivateGetBatteryDescriptionFunction
    : public ExtensionFunction {
 public:
  AccessibilityPrivateGetBatteryDescriptionFunction();
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.getBatteryDescription",
                             ACCESSIBILITY_PRIVATE_GETBATTERYDESCRIPTION)

 private:
  ~AccessibilityPrivateGetBatteryDescriptionFunction() override;
};

// API function that opens or closes the virtual keyboard.
class AccessibilityPrivateSetVirtualKeyboardVisibleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetVirtualKeyboardVisibleFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setVirtualKeyboardVisible",
                             ACCESSIBILITY_PRIVATE_SETVIRTUALKEYBOARDVISIBLE)
};
#endif  // defined (OS_CHROMEOS)

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
