// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_ASH_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_ASH_H_

// The functions in this file are alphabetized. Please insert new functions in
// alphabetical order.

#include <optional>

#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "extensions/browser/extension_function.h"

// API function that is called when the Select-to-Speak wants to perform a
// clipboard copy in a Lacros Google Doc.
class AccessibilityPrivateClipboardCopyInActiveLacrosGoogleDocFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateClipboardCopyInActiveLacrosGoogleDocFunction() override {
  }
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.clipboardCopyInActiveLacrosGoogleDoc",
      ACCESSIBILITY_PRIVATE_CLIPBOARDCOPYINACTIVELACROSGOOGLEDOC)
};

// API function that darkens or undarkens the screen.
class AccessibilityPrivateDarkenScreenFunction : public ExtensionFunction {
  ~AccessibilityPrivateDarkenScreenFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.darkenScreen",
                             ACCESSIBILITY_PRIVATE_DARKENSCREEN)
};

// API function that enables or disables mouse events in ChromeVox / Magnifier /
// FaceGaze.
class AccessibilityPrivateEnableMouseEventsFunction : public ExtensionFunction {
  ~AccessibilityPrivateEnableMouseEventsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.enableMouseEvents",
                             ACCESSIBILITY_PRIVATE_ENABLEMOUSEEVENTS)
};

// API function that sets the cursor position on the screen in absolute
// coordinates.
class AccessibilityPrivateSetCursorPositionFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetCursorPositionFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setCursorPosition",
                             ACCESSIBILITY_PRIVATE_SETCURSORPOSITION)
};

// API function that gets the bounds of the displays in absolute coordinates.
class AccessibilityPrivateGetDisplayBoundsFunction : public ExtensionFunction {
  ~AccessibilityPrivateGetDisplayBoundsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.getDisplayBounds",
                             ACCESSIBILITY_PRIVATE_GETDISPLAYBOUNDS)
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

// API function that retrieves DLC file contents.
class AccessibilityPrivateGetDlcContentsFunction : public ExtensionFunction {
  ~AccessibilityPrivateGetDlcContentsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.getDlcContents",
                             ACCESSIBILITY_PRIVATE_GETDLCCONTENTS)
 private:
  void OnDlcContentsRetrieved(const std::vector<uint8_t>& contents,
                              std::optional<std::string> error);
};

// API function that retrieves TTS DLC file contents.
class AccessibilityPrivateGetTtsDlcContentsFunction : public ExtensionFunction {
  ~AccessibilityPrivateGetTtsDlcContentsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.getTtsDlcContents",
                             ACCESSIBILITY_PRIVATE_GETTTSDLCCONTENTS)
 private:
  void OnTtsDlcContentsRetrieved(const std::vector<uint8_t>& contents,
                                 std::optional<std::string> error);
};

// API function that gets the localized DOM key string for a given key code.
class AccessibilityPrivateGetLocalizedDomKeyStringForKeyCodeFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateGetLocalizedDomKeyStringForKeyCodeFunction() override =
      default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.getLocalizedDomKeyStringForKeyCode",
      ACCESSIBILITY_PRIVATE_GETLOCALIZEDDOMKEYSTRINGFORKEYCODE)
};

// API function that is called when the Accessibility Common extension finds
// scrollable bounds.
class AccessibilityPrivateHandleScrollableBoundsForPointFoundFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateHandleScrollableBoundsForPointFoundFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.handleScrollableBoundsForPointFound",
      ACCESSIBILITY_PRIVATE_HANDLESCROLLABLEBOUNDSFORPOINTFOUND)
};

// API function that initiates a download of the FaceGaze assets DLC and
// responds with the file bytes via a callback.
class AccessibilityPrivateInstallFaceGazeAssetsFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateInstallFaceGazeAssetsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.installFaceGazeAssets",
                             ACCESSIBILITY_PRIVATE_INSTALLFACEGAZEASSETS)
 private:
  void OnInstallFinished(
      std::optional<::extensions::api::accessibility_private::FaceGazeAssets>
          assets);
};

// API function that sends gesture and confidence information for a detected
// gesture in FaceGaze.
class AccessibilityPrivateSendGestureInfoToSettingsFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendGestureInfoToSettingsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendGestureInfoToSettings",
                             ACCESSIBILITY_PRIVATE_SENDGESTUREINFOTOSETTINGS)
};

// API function that initiates a Pumpkin download for Dictation and responds
// with the file bytes via a callback.
class AccessibilityPrivateInstallPumpkinForDictationFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateInstallPumpkinForDictationFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.installPumpkinForDictation",
                             ACCESSIBILITY_PRIVATE_INSTALLPUMPKINFORDICTATION)
 private:
  void OnPumpkinInstallFinished(
      std::optional<::extensions::api::accessibility_private::PumpkinData>
          data);
};

// API function that determines if an accessibility feature is enabled.
class AccessibilityPrivateIsFeatureEnabledFunction : public ExtensionFunction {
  ~AccessibilityPrivateIsFeatureEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.isFeatureEnabled",
                             ACCESSIBILITY_PRIVATE_ISFEATUREENABLED)
};

// API function that is called by the Accessibility Common extension to center
// the magnifier viewport on a passed-in point.
class AccessibilityPrivateMagnifierCenterOnPointFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateMagnifierCenterOnPointFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.magnifierCenterOnPoint",
                             ACCESSIBILITY_PRIVATE_MAGNIFIERCENTERONPOINT)
};

// API function that is called by the Accessibility Common extension to center
// the magnifier viewport on a passed-in rect.
class AccessibilityPrivateMoveMagnifierToRectFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateMoveMagnifierToRectFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.moveMagnifierToRect",
                             ACCESSIBILITY_PRIVATE_MOVEMAGNIFIERTORECT)
};

// Opens a specified subpage in Chrome settings.
class AccessibilityPrivateOpenSettingsSubpageFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateOpenSettingsSubpageFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.openSettingsSubpage",
                             ACCESSIBILITY_PRIVATE_OPENSETTINGSSUBPAGE)
};

// API function that performs an accelerator action.
class AccessibilityPrivatePerformAcceleratorActionFunction
    : public ExtensionFunction {
  ~AccessibilityPrivatePerformAcceleratorActionFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.performAcceleratorAction",
                             ACCESSIBILITY_PRIVATE_PERFORMACCELERATORACTION)
};

// API function that injects key events.
class AccessibilityPrivateSendSyntheticKeyEventFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendSyntheticKeyEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticKeyEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICKEYEVENT)
};

// API function that injects mouse events.
class AccessibilityPrivateSendSyntheticMouseEventFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendSyntheticMouseEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticMouseEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICMOUSEEVENT)
};

// API function that scrolls at a point in the specified direction.
class AccessibilityPrivateScrollAtPointFunction : public ExtensionFunction {
  ~AccessibilityPrivateScrollAtPointFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.scrollAtPoint",
                             ACCESSIBILITY_PRIVATE_SCROLLATPOINT)
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

// API function that is called when the ChromeVox focus changes.
class AccessibilityPrivateSetChromeVoxFocusFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetChromeVoxFocusFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setChromeVoxFocus",
                             ACCESSIBILITY_PRIVATE_SETCHROMEVOXFOCUS)
};

// API function that is called when the Select to Speak reading position
// changes.
class AccessibilityPrivateSetSelectToSpeakFocusFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetSelectToSpeakFocusFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setSelectToSpeakFocus",
                             ACCESSIBILITY_PRIVATE_SETSELECTTOSPEAKFOCUS)
};

// API function that sets keyboard capture mode.
class AccessibilityPrivateSetKeyboardListenerFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetKeyboardListenerFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setKeyboardListener",
                             ACCESSIBILITY_PRIVATE_SETKEYBOARDLISTENER)
};

// API function that enables or disables web content accessibility support.
class AccessibilityPrivateSetNativeAccessibilityEnabledFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetNativeAccessibilityEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeAccessibilityEnabled",
      ACCESSIBILITY_PRIVATE_SETNATIVEACCESSIBILITYENABLED)
};

// API function that sets native ChromeVox ARC support.
class AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction()
      override {}
  ResponseAction Run() override;
  void OnResponse(
      extensions::api::accessibility_private::SetNativeChromeVoxResponse
          response);
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp",
      ACCESSIBILITY_PRIVATE_SETNATIVECHROMEVOXARCSUPPORTFORCURRENTAPP)
};

// API function that is called to start or end point scanning of the
// Switch Access extension.
class AccessibilityPrivateSetPointScanStateFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetPointScanStateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setPointScanState",
                             ACCESSIBILITY_PRIVATE_SETPOINTSCANSTATE)
};

// API function that is called when the Select-to-Speak extension state changes.
class AccessibilityPrivateSetSelectToSpeakStateFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetSelectToSpeakStateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setSelectToSpeakState",
                             ACCESSIBILITY_PRIVATE_SETSELECTTOSPEAKSTATE)
};

// API function that opens or closes the virtual keyboard.
class AccessibilityPrivateSetVirtualKeyboardVisibleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetVirtualKeyboardVisibleFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setVirtualKeyboardVisible",
                             ACCESSIBILITY_PRIVATE_SETVIRTUALKEYBOARDVISIBLE)
};

// API function that displays an accessibility-related toast.
class AccessibilityPrivateShowToastFunction : public ExtensionFunction {
  ~AccessibilityPrivateShowToastFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.showToast",
                             ACCESSIBILITY_PRIVATE_SHOWTOAST)
};

// API function that shows a confirmation dialog, with callbacks for
// confirm/cancel.
class AccessibilityPrivateShowConfirmationDialogFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateShowConfirmationDialogFunction() override = default;
  ResponseAction Run() override;
  void OnDialogResult(bool confirmed);
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.showConfirmationDialog",
                             ACCESSIBILITY_PRIVATE_SHOWCONFIRMATIONDIALOG)
};

// API function that silences ChromeVox.
class AccessibilityPrivateSilenceSpokenFeedbackFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSilenceSpokenFeedbackFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.silenceSpokenFeedback",
                             ACCESSIBILITY_PRIVATE_SILENCESPOKENFEEDBACK)
};

// API function that is called when a user toggles Dictation from another
// acessibility feature.
class AccessibilityPrivateToggleDictationFunction : public ExtensionFunction {
  ~AccessibilityPrivateToggleDictationFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.toggleDictation",
                             ACCESSIBILITY_PRIVATE_TOGGLEDICTATION)
};

// API function that updates the Dictation bubble UI.
class AccessibilityPrivateUpdateDictationBubbleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateUpdateDictationBubbleFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.updateDictationBubble",
                             ACCESSIBILITY_PRIVATE_UPDATEDICTATIONBUBBLE)
};

// API function that updates the FaceGaze bubble UI.
class AccessibilityPrivateUpdateFaceGazeBubbleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateUpdateFaceGazeBubbleFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.updateFaceGazeBubble",
                             ACCESSIBILITY_PRIVATE_UPDATEFACEGAZEBUBBLE)
};

// API function that updates properties of the Select-to-speak panel.
class AccessibilityPrivateUpdateSelectToSpeakPanelFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateUpdateSelectToSpeakPanelFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.updateSelectToSpeakPanel",
                             ACCESSIBILITY_PRIVATE_UPDATESELECTTOSPEAKPANEL)
};

// API function that is called to show or hide one of the Switch Access bubbles.
class AccessibilityPrivateUpdateSwitchAccessBubbleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateUpdateSwitchAccessBubbleFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.updateSwitchAccessBubble",
                             ACCESSIBILITY_PRIVATE_UPDATESWITCHACCESSBUBBLE)
};

class AccessibilityPrivateIsLacrosPrimaryFunction : public ExtensionFunction {
  ~AccessibilityPrivateIsLacrosPrimaryFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.isLacrosPrimary",
                             ACCESSIBILITY_PRIVATE_ISLACROSPRIMARY)
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_ASH_H_
