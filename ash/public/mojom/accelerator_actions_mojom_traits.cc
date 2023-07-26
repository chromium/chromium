// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/mojom/accelerator_actions_mojom_traits.h"

#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/mojom/accelerator_actions.mojom.h"
#include "base/notreached.h"

namespace mojo {

using mojom_acceleartor_action = ash::mojom::AcceleratorAction;

mojom_acceleartor_action
EnumTraits<mojom_acceleartor_action, ash::AcceleratorAction>::ToMojom(
    ash::AcceleratorAction accelerator_action) {
  switch (accelerator_action) {
    case ash::AcceleratorAction::kBrightnessDown:
      return mojom_acceleartor_action::kBrightnessDown;
    case ash::AcceleratorAction::kBrightnessUp:
      return mojom_acceleartor_action::kBrightnessUp;
    case ash::AcceleratorAction::kCycleBackwardMru:
      return mojom_acceleartor_action::kCycleBackwardMru;
    case ash::AcceleratorAction::kCycleForwardMru:
      return mojom_acceleartor_action::kCycleForwardMru;
    case ash::AcceleratorAction::kCycleSameAppWindowsBackward:
      return mojom_acceleartor_action::kCycleSameAppWindowsBackward;
    case ash::AcceleratorAction::kCycleSameAppWindowsForward:
      return mojom_acceleartor_action::kCycleSameAppWindowsForward;
    case ash::AcceleratorAction::kDesksActivateDeskLeft:
      return mojom_acceleartor_action::kDesksActivateDeskLeft;
    case ash::AcceleratorAction::kDesksActivateDeskRight:
      return mojom_acceleartor_action::kDesksActivateDeskRight;
    case ash::AcceleratorAction::kDesksMoveActiveItemLeft:
      return mojom_acceleartor_action::kDesksMoveActiveItemLeft;
    case ash::AcceleratorAction::kDesksMoveActiveItemRight:
      return mojom_acceleartor_action::kDesksMoveActiveItemRight;
    case ash::AcceleratorAction::kDesksNewDesk:
      return mojom_acceleartor_action::kDesksNewDesk;
    case ash::AcceleratorAction::kDesksRemoveCurrentDesk:
      return mojom_acceleartor_action::kDesksRemoveCurrentDesk;
    case ash::AcceleratorAction::kDesksActivate0:
      return mojom_acceleartor_action::kDesksActivate0;
    case ash::AcceleratorAction::kDesksActivate1:
      return mojom_acceleartor_action::kDesksActivate1;
    case ash::AcceleratorAction::kDesksActivate2:
      return mojom_acceleartor_action::kDesksActivate2;
    case ash::AcceleratorAction::kDesksActivate3:
      return mojom_acceleartor_action::kDesksActivate3;
    case ash::AcceleratorAction::kDesksActivate4:
      return mojom_acceleartor_action::kDesksActivate4;
    case ash::AcceleratorAction::kDesksActivate5:
      return mojom_acceleartor_action::kDesksActivate5;
    case ash::AcceleratorAction::kDesksActivate6:
      return mojom_acceleartor_action::kDesksActivate6;
    case ash::AcceleratorAction::kDesksActivate7:
      return mojom_acceleartor_action::kDesksActivate7;
    case ash::AcceleratorAction::kDesksToggleAssignToAllDesks:
      return mojom_acceleartor_action::kDesksToggleAssignToAllDesks;
    case ash::AcceleratorAction::kDisableCapsLock:
      return mojom_acceleartor_action::kDisableCapsLock;
    case ash::AcceleratorAction::kEnableOrToggleDictation:
      return mojom_acceleartor_action::kEnableOrToggleDictation;
    case ash::AcceleratorAction::kExit:
      return mojom_acceleartor_action::kExit;
    case ash::AcceleratorAction::kFocusCameraPreview:
      return mojom_acceleartor_action::kFocusCameraPreview;
    case ash::AcceleratorAction::kFocusNextPane:
      return mojom_acceleartor_action::kFocusNextPane;
    case ash::AcceleratorAction::kFocusPreviousPane:
      return mojom_acceleartor_action::kFocusPreviousPane;
    case ash::AcceleratorAction::kFocusShelf:
      return mojom_acceleartor_action::kFocusShelf;
    case ash::AcceleratorAction::kFocusPip:
      return mojom_acceleartor_action::kFocusPip;
    case ash::AcceleratorAction::kKeyboardBacklightToggle:
      return mojom_acceleartor_action::kKeyboardBacklightToggle;
    case ash::AcceleratorAction::kKeyboardBrightnessDown:
      return mojom_acceleartor_action::kKeyboardBrightnessDown;
    case ash::AcceleratorAction::kKeyboardBrightnessUp:
      return mojom_acceleartor_action::kKeyboardBrightnessUp;
    case ash::AcceleratorAction::kLaunchApp0:
      return mojom_acceleartor_action::kLaunchApp0;
    case ash::AcceleratorAction::kLaunchApp1:
      return mojom_acceleartor_action::kLaunchApp1;
    case ash::AcceleratorAction::kLaunchApp2:
      return mojom_acceleartor_action::kLaunchApp2;
    case ash::AcceleratorAction::kLaunchApp3:
      return mojom_acceleartor_action::kLaunchApp3;
    case ash::AcceleratorAction::kLaunchApp4:
      return mojom_acceleartor_action::kLaunchApp4;
    case ash::AcceleratorAction::kLaunchApp5:
      return mojom_acceleartor_action::kLaunchApp5;
    case ash::AcceleratorAction::kLaunchApp6:
      return mojom_acceleartor_action::kLaunchApp6;
    case ash::AcceleratorAction::kLaunchApp7:
      return mojom_acceleartor_action::kLaunchApp7;
    case ash::AcceleratorAction::kLaunchLastApp:
      return mojom_acceleartor_action::kLaunchLastApp;
    case ash::AcceleratorAction::kLockPressed:
      return mojom_acceleartor_action::kLockPressed;
    case ash::AcceleratorAction::kLockReleased:
      return mojom_acceleartor_action::kLockReleased;
    case ash::AcceleratorAction::kLockScreen:
      return mojom_acceleartor_action::kLockScreen;
    case ash::AcceleratorAction::kMagnifierZoomIn:
      return mojom_acceleartor_action::kMagnifierZoomIn;
    case ash::AcceleratorAction::kMagnifierZoomOut:
      return mojom_acceleartor_action::kMagnifierZoomOut;
    case ash::AcceleratorAction::kMediaFastForward:
      return mojom_acceleartor_action::kMediaFastForward;
    case ash::AcceleratorAction::kMediaNextTrack:
      return mojom_acceleartor_action::kMediaNextTrack;
    case ash::AcceleratorAction::kMediaPause:
      return mojom_acceleartor_action::kMediaPause;
    case ash::AcceleratorAction::kMediaPlay:
      return mojom_acceleartor_action::kMediaPlay;
    case ash::AcceleratorAction::kMediaPlayPause:
      return mojom_acceleartor_action::kMediaPlayPause;
    case ash::AcceleratorAction::kMediaPrevTrack:
      return mojom_acceleartor_action::kMediaPrevTrack;
    case ash::AcceleratorAction::kMediaRewind:
      return mojom_acceleartor_action::kMediaRewind;
    case ash::AcceleratorAction::kMediaStop:
      return mojom_acceleartor_action::kMediaStop;
    case ash::AcceleratorAction::kMicrophoneMuteToggle:
      return mojom_acceleartor_action::kMicrophoneMuteToggle;
    case ash::AcceleratorAction::kMoveActiveWindowBetweenDisplays:
      return mojom_acceleartor_action::kMoveActiveWindowBetweenDisplays;
    case ash::AcceleratorAction::kNewIncognitoWindow:
      return mojom_acceleartor_action::kNewIncognitoWindow;
    case ash::AcceleratorAction::kNewTab:
      return mojom_acceleartor_action::kNewTab;
    case ash::AcceleratorAction::kNewWindow:
      return mojom_acceleartor_action::kNewWindow;
    case ash::AcceleratorAction::kOpenCalculator:
      return mojom_acceleartor_action::kOpenCalculator;
    case ash::AcceleratorAction::kOpenCrosh:
      return mojom_acceleartor_action::kOpenCrosh;
    case ash::AcceleratorAction::kOpenDiagnostics:
      return mojom_acceleartor_action::kOpenDiagnostics;
    case ash::AcceleratorAction::kOpenFeedbackPage:
      return mojom_acceleartor_action::kOpenFeedbackPage;
    case ash::AcceleratorAction::kOpenFileManager:
      return mojom_acceleartor_action::kOpenFileManager;
    case ash::AcceleratorAction::kOpenGetHelp:
      return mojom_acceleartor_action::kOpenGetHelp;
    case ash::AcceleratorAction::kPasteClipboardHistoryPlainText:
      return mojom_acceleartor_action::kPasteClipboardHistoryPlainText;
    case ash::AcceleratorAction::kPowerPressed:
      return mojom_acceleartor_action::kPowerPressed;
    case ash::AcceleratorAction::kPowerReleased:
      return mojom_acceleartor_action::kPowerReleased;
    case ash::AcceleratorAction::kPrintUiHierarchies:
      return mojom_acceleartor_action::kPrintUiHierarchies;
    case ash::AcceleratorAction::kPrivacyScreenToggle:
      return mojom_acceleartor_action::kPrivacyScreenToggle;
    case ash::AcceleratorAction::kRestoreTab:
      return mojom_acceleartor_action::kRestoreTab;
    case ash::AcceleratorAction::kRotateScreen:
      return mojom_acceleartor_action::kRotateScreen;
    case ash::AcceleratorAction::kRotateWindow:
      return mojom_acceleartor_action::kRotateWindow;
    case ash::AcceleratorAction::kScaleUiDown:
      return mojom_acceleartor_action::kScaleUiDown;
    case ash::AcceleratorAction::kScaleUiReset:
      return mojom_acceleartor_action::kScaleUiReset;
    case ash::AcceleratorAction::kScaleUiUp:
      return mojom_acceleartor_action::kScaleUiUp;
    case ash::AcceleratorAction::kShowEmojiPicker:
      return mojom_acceleartor_action::kShowEmojiPicker;
    case ash::AcceleratorAction::kToggleImeMenuBubble:
      return mojom_acceleartor_action::kToggleImeMenuBubble;
    case ash::AcceleratorAction::kShowShortcutViewer:
      return mojom_acceleartor_action::kShowShortcutViewer;
    case ash::AcceleratorAction::kShowStylusTools:
      return mojom_acceleartor_action::kShowStylusTools;
    case ash::AcceleratorAction::kShowTaskManager:
      return mojom_acceleartor_action::kShowTaskManager;
    case ash::AcceleratorAction::kStartAssistant:
      return mojom_acceleartor_action::kStartAssistant;
    case ash::AcceleratorAction::kStopScreenRecording:
      return mojom_acceleartor_action::kStopScreenRecording;
    case ash::AcceleratorAction::kSuspend:
      return mojom_acceleartor_action::kSuspend;
    case ash::AcceleratorAction::kSwapPrimaryDisplay:
      return mojom_acceleartor_action::kSwapPrimaryDisplay;
    case ash::AcceleratorAction::kSwitchIme:
      return mojom_acceleartor_action::kSwitchIme;
    case ash::AcceleratorAction::kSwitchToLastUsedIme:
      return mojom_acceleartor_action::kSwitchToLastUsedIme;
    case ash::AcceleratorAction::kSwitchToNextIme:
      return mojom_acceleartor_action::kSwitchToNextIme;
    case ash::AcceleratorAction::kSwitchToNextUser:
      return mojom_acceleartor_action::kSwitchToNextUser;
    case ash::AcceleratorAction::kSwitchToPreviousUser:
      return mojom_acceleartor_action::kSwitchToPreviousUser;
    case ash::AcceleratorAction::kTakePartialScreenshot:
      return mojom_acceleartor_action::kTakePartialScreenshot;
    case ash::AcceleratorAction::kTakeScreenshot:
      return mojom_acceleartor_action::kTakeScreenshot;
    case ash::AcceleratorAction::kTakeWindowScreenshot:
      return mojom_acceleartor_action::kTakeWindowScreenshot;
    case ash::AcceleratorAction::kToggleAppList:
      return mojom_acceleartor_action::kToggleAppList;
    case ash::AcceleratorAction::kToggleCalendar:
      return mojom_acceleartor_action::kToggleCalendar;
    case ash::AcceleratorAction::kToggleCapsLock:
      return mojom_acceleartor_action::kToggleCapsLock;
    case ash::AcceleratorAction::kToggleClipboardHistory:
      return mojom_acceleartor_action::kToggleClipboardHistory;
    case ash::AcceleratorAction::kToggleDockedMagnifier:
      return mojom_acceleartor_action::kToggleDockedMagnifier;
    case ash::AcceleratorAction::kToggleFloating:
      return mojom_acceleartor_action::kToggleFloating;
    case ash::AcceleratorAction::kToggleFullscreen:
      return mojom_acceleartor_action::kToggleFullscreen;
    case ash::AcceleratorAction::kToggleFullscreenMagnifier:
      return mojom_acceleartor_action::kToggleFullscreenMagnifier;
    case ash::AcceleratorAction::kToggleGameDashboard:
      return mojom_acceleartor_action::kToggleGameDashboard;
    case ash::AcceleratorAction::kToggleHighContrast:
      return mojom_acceleartor_action::kToggleHighContrast;
    case ash::AcceleratorAction::kToggleMaximized:
      return mojom_acceleartor_action::kToggleMaximized;
    case ash::AcceleratorAction::kToggleMessageCenterBubble:
      return mojom_acceleartor_action::kToggleMessageCenterBubble;
    case ash::AcceleratorAction::kToggleMirrorMode:
      return mojom_acceleartor_action::kToggleMirrorMode;
    case ash::AcceleratorAction::kToggleMultitaskMenu:
      return mojom_acceleartor_action::kToggleMultitaskMenu;
    case ash::AcceleratorAction::kToggleOverview:
      return mojom_acceleartor_action::kToggleOverview;
    case ash::AcceleratorAction::kToggleProjectorMarker:
      return mojom_acceleartor_action::kToggleProjectorMarker;
    case ash::AcceleratorAction::kToggleResizeLockMenu:
      return mojom_acceleartor_action::kToggleResizeLockMenu;
    case ash::AcceleratorAction::kToggleSnapGroupWindowsGroupAndUngroup:
      return mojom_acceleartor_action::kToggleSnapGroupWindowsGroupAndUngroup;
    case ash::AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore:
      return mojom_acceleartor_action::
          kToggleSnapGroupWindowsMinimizeAndRestore;
    case ash::AcceleratorAction::kToggleSpokenFeedback:
      return mojom_acceleartor_action::kToggleSpokenFeedback;
    case ash::AcceleratorAction::kToggleSystemTrayBubble:
      return mojom_acceleartor_action::kToggleSystemTrayBubble;
    case ash::AcceleratorAction::kToggleWifi:
      return mojom_acceleartor_action::kToggleWifi;
    case ash::AcceleratorAction::kTouchHudClear:
      return mojom_acceleartor_action::kTouchHudClear;
    case ash::AcceleratorAction::kTouchHudModeChange:
      return mojom_acceleartor_action::kTouchHudModeChange;
    case ash::AcceleratorAction::kTouchFingerprintSensor1:
      return mojom_acceleartor_action::kTouchFingerprintSensor1;
    case ash::AcceleratorAction::kTouchFingerprintSensor2:
      return mojom_acceleartor_action::kTouchFingerprintSensor2;
    case ash::AcceleratorAction::kTouchFingerprintSensor3:
      return mojom_acceleartor_action::kTouchFingerprintSensor3;
    case ash::AcceleratorAction::kUnpin:
      return mojom_acceleartor_action::kUnpin;
    case ash::AcceleratorAction::kVolumeDown:
      return mojom_acceleartor_action::kVolumeDown;
    case ash::AcceleratorAction::kVolumeMute:
      return mojom_acceleartor_action::kVolumeMute;
    case ash::AcceleratorAction::kVolumeUp:
      return mojom_acceleartor_action::kVolumeUp;
    case ash::AcceleratorAction::kWindowCycleSnapLeft:
      return mojom_acceleartor_action::kWindowCycleSnapLeft;
    case ash::AcceleratorAction::kWindowCycleSnapRight:
      return mojom_acceleartor_action::kWindowCycleSnapRight;
    case ash::AcceleratorAction::kWindowMinimize:
      return mojom_acceleartor_action::kWindowMinimize;
    case ash::AcceleratorAction::kMinimizeTopWindowOnBack:
      return mojom_acceleartor_action::kMinimizeTopWindowOnBack;
    case ash::AcceleratorAction::kDebugClearUseKMeansPref:
      return mojom_acceleartor_action::kDebugClearUseKMeansPref;
    case ash::AcceleratorAction::kDebugKeyboardBacklightToggle:
      return mojom_acceleartor_action::kDebugKeyboardBacklightToggle;
    case ash::AcceleratorAction::kDebugMicrophoneMuteToggle:
      return mojom_acceleartor_action::kDebugMicrophoneMuteToggle;
    case ash::AcceleratorAction::kDebugPrintLayerHierarchy:
      return mojom_acceleartor_action::kDebugPrintLayerHierarchy;
    case ash::AcceleratorAction::kDebugPrintViewHierarchy:
      return mojom_acceleartor_action::kDebugPrintViewHierarchy;
    case ash::AcceleratorAction::kDebugPrintWindowHierarchy:
      return mojom_acceleartor_action::kDebugPrintWindowHierarchy;
    case ash::AcceleratorAction::kDebugShowToast:
      return mojom_acceleartor_action::kDebugShowToast;
    case ash::AcceleratorAction::kDebugSystemUiStyleViewer:
      return mojom_acceleartor_action::kDebugSystemUiStyleViewer;
    case ash::AcceleratorAction::kDebugToggleDarkMode:
      return mojom_acceleartor_action::kDebugToggleDarkMode;
    case ash::AcceleratorAction::kDebugToggleDynamicColor:
      return mojom_acceleartor_action::kDebugToggleDynamicColor;
    case ash::AcceleratorAction::kDebugToggleGlanceables:
      return mojom_acceleartor_action::kDebugToggleGlanceables;
    case ash::AcceleratorAction::kDebugTogglePowerButtonMenu:
      return mojom_acceleartor_action::kDebugTogglePowerButtonMenu;
    case ash::AcceleratorAction::kDebugToggleShowDebugBorders:
      return mojom_acceleartor_action::kDebugToggleShowDebugBorders;
    case ash::AcceleratorAction::kDebugToggleShowFpsCounter:
      return mojom_acceleartor_action::kDebugToggleShowFpsCounter;
    case ash::AcceleratorAction::kDebugToggleShowPaintRects:
      return mojom_acceleartor_action::kDebugToggleShowPaintRects;
    case ash::AcceleratorAction::kDebugToggleTouchPad:
      return mojom_acceleartor_action::kDebugToggleTouchPad;
    case ash::AcceleratorAction::kDebugToggleTouchScreen:
      return mojom_acceleartor_action::kDebugToggleTouchScreen;
    case ash::AcceleratorAction::kDebugToggleTabletMode:
      return mojom_acceleartor_action::kDebugToggleTabletMode;
    case ash::AcceleratorAction::kDebugToggleVideoConferenceCameraTrayIcon:
      return mojom_acceleartor_action::
          kDebugToggleVideoConferenceCameraTrayIcon;
    case ash::AcceleratorAction::kDebugToggleWallpaperMode:
      return mojom_acceleartor_action::kDebugToggleWallpaperMode;
    case ash::AcceleratorAction::kDebugTriggerCrash:
      return mojom_acceleartor_action::kDebugTriggerCrash;
    case ash::AcceleratorAction::kDebugToggleHudDisplay:
      return mojom_acceleartor_action::kDebugToggleHudDisplay;
    case ash::AcceleratorAction::kDebugToggleVirtualTrackpad:
      return mojom_acceleartor_action::kDebugToggleVirtualTrackpad;
    case ash::AcceleratorAction::kDevAddRemoveDisplay:
      return mojom_acceleartor_action::kDevAddRemoveDisplay;
    case ash::AcceleratorAction::kDevToggleAppList:
      return mojom_acceleartor_action::kDevToggleAppList;
    case ash::AcceleratorAction::kDevToggleUnifiedDesktop:
      return mojom_acceleartor_action::kDevToggleUnifiedDesktop;
  }

  NOTREACHED_NORETURN();
}

bool EnumTraits<mojom_acceleartor_action, ash::AcceleratorAction>::FromMojom(
    ash::mojom::AcceleratorAction input,
    ash::AcceleratorAction* out) {
  switch (input) {
    case mojom_acceleartor_action::kBrightnessDown:
      *out = ash::AcceleratorAction::kBrightnessDown;
      return true;
    case mojom_acceleartor_action::kBrightnessUp:
      *out = ash::AcceleratorAction::kBrightnessUp;
      return true;
    case mojom_acceleartor_action::kCycleBackwardMru:
      *out = ash::AcceleratorAction::kCycleBackwardMru;
      return true;
    case mojom_acceleartor_action::kCycleForwardMru:
      *out = ash::AcceleratorAction::kCycleForwardMru;
      return true;
    case mojom_acceleartor_action::kCycleSameAppWindowsBackward:
      *out = ash::AcceleratorAction::kCycleSameAppWindowsBackward;
      return true;
    case mojom_acceleartor_action::kCycleSameAppWindowsForward:
      *out = ash::AcceleratorAction::kCycleSameAppWindowsForward;
      return true;
    case mojom_acceleartor_action::kDesksActivateDeskLeft:
      *out = ash::AcceleratorAction::kDesksActivateDeskLeft;
      return true;
    case mojom_acceleartor_action::kDesksActivateDeskRight:
      *out = ash::AcceleratorAction::kDesksActivateDeskRight;
      return true;
    case mojom_acceleartor_action::kDesksMoveActiveItemLeft:
      *out = ash::AcceleratorAction::kDesksMoveActiveItemLeft;
      return true;
    case mojom_acceleartor_action::kDesksMoveActiveItemRight:
      *out = ash::AcceleratorAction::kDesksMoveActiveItemRight;
      return true;
    case mojom_acceleartor_action::kDesksNewDesk:
      *out = ash::AcceleratorAction::kDesksNewDesk;
      return true;
    case mojom_acceleartor_action::kDesksRemoveCurrentDesk:
      *out = ash::AcceleratorAction::kDesksRemoveCurrentDesk;
      return true;
    case mojom_acceleartor_action::kDesksActivate0:
      *out = ash::AcceleratorAction::kDesksActivate0;
      return true;
    case mojom_acceleartor_action::kDesksActivate1:
      *out = ash::AcceleratorAction::kDesksActivate1;
      return true;
    case mojom_acceleartor_action::kDesksActivate2:
      *out = ash::AcceleratorAction::kDesksActivate2;
      return true;
    case mojom_acceleartor_action::kDesksActivate3:
      *out = ash::AcceleratorAction::kDesksActivate3;
      return true;
    case mojom_acceleartor_action::kDesksActivate4:
      *out = ash::AcceleratorAction::kDesksActivate4;
      return true;
    case mojom_acceleartor_action::kDesksActivate5:
      *out = ash::AcceleratorAction::kDesksActivate5;
      return true;
    case mojom_acceleartor_action::kDesksActivate6:
      *out = ash::AcceleratorAction::kDesksActivate6;
      return true;
    case mojom_acceleartor_action::kDesksActivate7:
      *out = ash::AcceleratorAction::kDesksActivate7;
      return true;
    case mojom_acceleartor_action::kDesksToggleAssignToAllDesks:
      *out = ash::AcceleratorAction::kDesksToggleAssignToAllDesks;
      return true;
    case mojom_acceleartor_action::kDisableCapsLock:
      *out = ash::AcceleratorAction::kDisableCapsLock;
      return true;
    case mojom_acceleartor_action::kEnableOrToggleDictation:
      *out = ash::AcceleratorAction::kEnableOrToggleDictation;
      return true;
    case mojom_acceleartor_action::kExit:
      *out = ash::AcceleratorAction::kExit;
      return true;
    case mojom_acceleartor_action::kFocusCameraPreview:
      *out = ash::AcceleratorAction::kFocusCameraPreview;
      return true;
    case mojom_acceleartor_action::kFocusNextPane:
      *out = ash::AcceleratorAction::kFocusNextPane;
      return true;
    case mojom_acceleartor_action::kFocusPreviousPane:
      *out = ash::AcceleratorAction::kFocusPreviousPane;
      return true;
    case mojom_acceleartor_action::kFocusShelf:
      *out = ash::AcceleratorAction::kFocusShelf;
      return true;
    case mojom_acceleartor_action::kFocusPip:
      *out = ash::AcceleratorAction::kFocusPip;
      return true;
    case mojom_acceleartor_action::kKeyboardBacklightToggle:
      *out = ash::AcceleratorAction::kKeyboardBacklightToggle;
      return true;
    case mojom_acceleartor_action::kKeyboardBrightnessDown:
      *out = ash::AcceleratorAction::kKeyboardBrightnessDown;
      return true;
    case mojom_acceleartor_action::kKeyboardBrightnessUp:
      *out = ash::AcceleratorAction::kKeyboardBrightnessUp;
      return true;
    case mojom_acceleartor_action::kLaunchApp0:
      *out = ash::AcceleratorAction::kLaunchApp0;
      return true;
    case mojom_acceleartor_action::kLaunchApp1:
      *out = ash::AcceleratorAction::kLaunchApp1;
      return true;
    case mojom_acceleartor_action::kLaunchApp2:
      *out = ash::AcceleratorAction::kLaunchApp2;
      return true;
    case mojom_acceleartor_action::kLaunchApp3:
      *out = ash::AcceleratorAction::kLaunchApp3;
      return true;
    case mojom_acceleartor_action::kLaunchApp4:
      *out = ash::AcceleratorAction::kLaunchApp4;
      return true;
    case mojom_acceleartor_action::kLaunchApp5:
      *out = ash::AcceleratorAction::kLaunchApp5;
      return true;
    case mojom_acceleartor_action::kLaunchApp6:
      *out = ash::AcceleratorAction::kLaunchApp6;
      return true;
    case mojom_acceleartor_action::kLaunchApp7:
      *out = ash::AcceleratorAction::kLaunchApp7;
      return true;
    case mojom_acceleartor_action::kLaunchLastApp:
      *out = ash::AcceleratorAction::kLaunchLastApp;
      return true;
    case mojom_acceleartor_action::kLockPressed:
      *out = ash::AcceleratorAction::kLockPressed;
      return true;
    case mojom_acceleartor_action::kLockReleased:
      *out = ash::AcceleratorAction::kLockReleased;
      return true;
    case mojom_acceleartor_action::kLockScreen:
      *out = ash::AcceleratorAction::kLockScreen;
      return true;
    case mojom_acceleartor_action::kMagnifierZoomIn:
      *out = ash::AcceleratorAction::kMagnifierZoomIn;
      return true;
    case mojom_acceleartor_action::kMagnifierZoomOut:
      *out = ash::AcceleratorAction::kMagnifierZoomOut;
      return true;
    case mojom_acceleartor_action::kMediaFastForward:
      *out = ash::AcceleratorAction::kMediaFastForward;
      return true;
    case mojom_acceleartor_action::kMediaNextTrack:
      *out = ash::AcceleratorAction::kMediaNextTrack;
      return true;
    case mojom_acceleartor_action::kMediaPause:
      *out = ash::AcceleratorAction::kMediaPause;
      return true;
    case mojom_acceleartor_action::kMediaPlay:
      *out = ash::AcceleratorAction::kMediaPlay;
      return true;
    case mojom_acceleartor_action::kMediaPlayPause:
      *out = ash::AcceleratorAction::kMediaPlayPause;
      return true;
    case mojom_acceleartor_action::kMediaPrevTrack:
      *out = ash::AcceleratorAction::kMediaPrevTrack;
      return true;
    case mojom_acceleartor_action::kMediaRewind:
      *out = ash::AcceleratorAction::kMediaRewind;
      return true;
    case mojom_acceleartor_action::kMediaStop:
      *out = ash::AcceleratorAction::kMediaStop;
      return true;
    case mojom_acceleartor_action::kMicrophoneMuteToggle:
      *out = ash::AcceleratorAction::kMicrophoneMuteToggle;
      return true;
    case mojom_acceleartor_action::kMoveActiveWindowBetweenDisplays:
      *out = ash::AcceleratorAction::kMoveActiveWindowBetweenDisplays;
      return true;
    case mojom_acceleartor_action::kNewIncognitoWindow:
      *out = ash::AcceleratorAction::kNewIncognitoWindow;
      return true;
    case mojom_acceleartor_action::kNewTab:
      *out = ash::AcceleratorAction::kNewTab;
      return true;
    case mojom_acceleartor_action::kNewWindow:
      *out = ash::AcceleratorAction::kNewWindow;
      return true;
    case mojom_acceleartor_action::kOpenCalculator:
      *out = ash::AcceleratorAction::kOpenCalculator;
      return true;
    case mojom_acceleartor_action::kOpenCrosh:
      *out = ash::AcceleratorAction::kOpenCrosh;
      return true;
    case mojom_acceleartor_action::kOpenDiagnostics:
      *out = ash::AcceleratorAction::kOpenDiagnostics;
      return true;
    case mojom_acceleartor_action::kOpenFeedbackPage:
      *out = ash::AcceleratorAction::kOpenFeedbackPage;
      return true;
    case mojom_acceleartor_action::kOpenFileManager:
      *out = ash::AcceleratorAction::kOpenFileManager;
      return true;
    case mojom_acceleartor_action::kOpenGetHelp:
      *out = ash::AcceleratorAction::kOpenGetHelp;
      return true;
    case mojom_acceleartor_action::kPasteClipboardHistoryPlainText:
      *out = ash::AcceleratorAction::kPasteClipboardHistoryPlainText;
      return true;
    case mojom_acceleartor_action::kPowerPressed:
      *out = ash::AcceleratorAction::kPowerPressed;
      return true;
    case mojom_acceleartor_action::kPowerReleased:
      *out = ash::AcceleratorAction::kPowerReleased;
      return true;
    case mojom_acceleartor_action::kPrintUiHierarchies:
      *out = ash::AcceleratorAction::kPrintUiHierarchies;
      return true;
    case mojom_acceleartor_action::kPrivacyScreenToggle:
      *out = ash::AcceleratorAction::kPrivacyScreenToggle;
      return true;
    case mojom_acceleartor_action::kRestoreTab:
      *out = ash::AcceleratorAction::kRestoreTab;
      return true;
    case mojom_acceleartor_action::kRotateScreen:
      *out = ash::AcceleratorAction::kRotateScreen;
      return true;
    case mojom_acceleartor_action::kRotateWindow:
      *out = ash::AcceleratorAction::kRotateWindow;
      return true;
    case mojom_acceleartor_action::kScaleUiDown:
      *out = ash::AcceleratorAction::kScaleUiDown;
      return true;
    case mojom_acceleartor_action::kScaleUiReset:
      *out = ash::AcceleratorAction::kScaleUiReset;
      return true;
    case mojom_acceleartor_action::kScaleUiUp:
      *out = ash::AcceleratorAction::kScaleUiUp;
      return true;
    case mojom_acceleartor_action::kShowEmojiPicker:
      *out = ash::AcceleratorAction::kShowEmojiPicker;
      return true;
    case mojom_acceleartor_action::kToggleImeMenuBubble:
      *out = ash::AcceleratorAction::kToggleImeMenuBubble;
      return true;
    case mojom_acceleartor_action::kShowShortcutViewer:
      *out = ash::AcceleratorAction::kShowShortcutViewer;
      return true;
    case mojom_acceleartor_action::kShowStylusTools:
      *out = ash::AcceleratorAction::kShowStylusTools;
      return true;
    case mojom_acceleartor_action::kShowTaskManager:
      *out = ash::AcceleratorAction::kShowTaskManager;
      return true;
    case mojom_acceleartor_action::kStartAssistant:
      *out = ash::AcceleratorAction::kStartAssistant;
      return true;
    case mojom_acceleartor_action::kStopScreenRecording:
      *out = ash::AcceleratorAction::kStopScreenRecording;
      return true;
    case mojom_acceleartor_action::kSuspend:
      *out = ash::AcceleratorAction::kSuspend;
      return true;
    case mojom_acceleartor_action::kSwapPrimaryDisplay:
      *out = ash::AcceleratorAction::kSwapPrimaryDisplay;
      return true;
    case mojom_acceleartor_action::kSwitchIme:
      *out = ash::AcceleratorAction::kSwitchIme;
      return true;
    case mojom_acceleartor_action::kSwitchToLastUsedIme:
      *out = ash::AcceleratorAction::kSwitchToLastUsedIme;
      return true;
    case mojom_acceleartor_action::kSwitchToNextIme:
      *out = ash::AcceleratorAction::kSwitchToNextIme;
      return true;
    case mojom_acceleartor_action::kSwitchToNextUser:
      *out = ash::AcceleratorAction::kSwitchToNextUser;
      return true;
    case mojom_acceleartor_action::kSwitchToPreviousUser:
      *out = ash::AcceleratorAction::kSwitchToPreviousUser;
      return true;
    case mojom_acceleartor_action::kTakePartialScreenshot:
      *out = ash::AcceleratorAction::kTakePartialScreenshot;
      return true;
    case mojom_acceleartor_action::kTakeScreenshot:
      *out = ash::AcceleratorAction::kTakeScreenshot;
      return true;
    case mojom_acceleartor_action::kTakeWindowScreenshot:
      *out = ash::AcceleratorAction::kTakeWindowScreenshot;
      return true;
    case mojom_acceleartor_action::kToggleAppList:
      *out = ash::AcceleratorAction::kToggleAppList;
      return true;
    case mojom_acceleartor_action::kToggleCalendar:
      *out = ash::AcceleratorAction::kToggleCalendar;
      return true;
    case mojom_acceleartor_action::kToggleCapsLock:
      *out = ash::AcceleratorAction::kToggleCapsLock;
      return true;
    case mojom_acceleartor_action::kToggleClipboardHistory:
      *out = ash::AcceleratorAction::kToggleClipboardHistory;
      return true;
    case mojom_acceleartor_action::kToggleDockedMagnifier:
      *out = ash::AcceleratorAction::kToggleDockedMagnifier;
      return true;
    case mojom_acceleartor_action::kToggleFloating:
      *out = ash::AcceleratorAction::kToggleFloating;
      return true;
    case mojom_acceleartor_action::kToggleFullscreen:
      *out = ash::AcceleratorAction::kToggleFullscreen;
      return true;
    case mojom_acceleartor_action::kToggleFullscreenMagnifier:
      *out = ash::AcceleratorAction::kToggleFullscreenMagnifier;
      return true;
    case mojom_acceleartor_action::kToggleGameDashboard:
      *out = ash::AcceleratorAction::kToggleGameDashboard;
      return true;
    case mojom_acceleartor_action::kToggleHighContrast:
      *out = ash::AcceleratorAction::kToggleHighContrast;
      return true;
    case mojom_acceleartor_action::kToggleMaximized:
      *out = ash::AcceleratorAction::kToggleMaximized;
      return true;
    case mojom_acceleartor_action::kToggleMessageCenterBubble:
      *out = ash::AcceleratorAction::kToggleMessageCenterBubble;
      return true;
    case mojom_acceleartor_action::kToggleMirrorMode:
      *out = ash::AcceleratorAction::kToggleMirrorMode;
      return true;
    case mojom_acceleartor_action::kToggleMultitaskMenu:
      *out = ash::AcceleratorAction::kToggleMultitaskMenu;
      return true;
    case mojom_acceleartor_action::kToggleOverview:
      *out = ash::AcceleratorAction::kToggleOverview;
      return true;
    case mojom_acceleartor_action::kToggleProjectorMarker:
      *out = ash::AcceleratorAction::kToggleProjectorMarker;
      return true;
    case mojom_acceleartor_action::kToggleResizeLockMenu:
      *out = ash::AcceleratorAction::kToggleResizeLockMenu;
      return true;
    case mojom_acceleartor_action::kToggleSnapGroupWindowsGroupAndUngroup:
      *out = ash::AcceleratorAction::kToggleSnapGroupWindowsGroupAndUngroup;
      return true;
    case mojom_acceleartor_action::kToggleSnapGroupWindowsMinimizeAndRestore:
      *out = ash::AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore;
      return true;
    case mojom_acceleartor_action::kToggleSpokenFeedback:
      *out = ash::AcceleratorAction::kToggleSpokenFeedback;
      return true;
    case mojom_acceleartor_action::kToggleSystemTrayBubble:
      *out = ash::AcceleratorAction::kToggleSystemTrayBubble;
      return true;
    case mojom_acceleartor_action::kToggleWifi:
      *out = ash::AcceleratorAction::kToggleWifi;
      return true;
    case mojom_acceleartor_action::kTouchHudClear:
      *out = ash::AcceleratorAction::kTouchHudClear;
      return true;
    case mojom_acceleartor_action::kTouchHudModeChange:
      *out = ash::AcceleratorAction::kTouchHudModeChange;
      return true;
    case mojom_acceleartor_action::kTouchFingerprintSensor1:
      *out = ash::AcceleratorAction::kTouchFingerprintSensor1;
      return true;
    case mojom_acceleartor_action::kTouchFingerprintSensor2:
      *out = ash::AcceleratorAction::kTouchFingerprintSensor2;
      return true;
    case mojom_acceleartor_action::kTouchFingerprintSensor3:
      *out = ash::AcceleratorAction::kTouchFingerprintSensor3;
      return true;
    case mojom_acceleartor_action::kUnpin:
      *out = ash::AcceleratorAction::kUnpin;
      return true;
    case mojom_acceleartor_action::kVolumeDown:
      *out = ash::AcceleratorAction::kVolumeDown;
      return true;
    case mojom_acceleartor_action::kVolumeMute:
      *out = ash::AcceleratorAction::kVolumeMute;
      return true;
    case mojom_acceleartor_action::kVolumeUp:
      *out = ash::AcceleratorAction::kVolumeUp;
      return true;
    case mojom_acceleartor_action::kWindowCycleSnapLeft:
      *out = ash::AcceleratorAction::kWindowCycleSnapLeft;
      return true;
    case mojom_acceleartor_action::kWindowCycleSnapRight:
      *out = ash::AcceleratorAction::kWindowCycleSnapRight;
      return true;
    case mojom_acceleartor_action::kWindowMinimize:
      *out = ash::AcceleratorAction::kWindowMinimize;
      return true;
    case mojom_acceleartor_action::kMinimizeTopWindowOnBack:
      *out = ash::AcceleratorAction::kMinimizeTopWindowOnBack;
      return true;
    case mojom_acceleartor_action::kDebugClearUseKMeansPref:
      *out = ash::AcceleratorAction::kDebugClearUseKMeansPref;
      return true;
    case mojom_acceleartor_action::kDebugKeyboardBacklightToggle:
      *out = ash::AcceleratorAction::kDebugKeyboardBacklightToggle;
      return true;
    case mojom_acceleartor_action::kDebugMicrophoneMuteToggle:
      *out = ash::AcceleratorAction::kDebugMicrophoneMuteToggle;
      return true;
    case mojom_acceleartor_action::kDebugPrintLayerHierarchy:
      *out = ash::AcceleratorAction::kDebugPrintLayerHierarchy;
      return true;
    case mojom_acceleartor_action::kDebugPrintViewHierarchy:
      *out = ash::AcceleratorAction::kDebugPrintViewHierarchy;
      return true;
    case mojom_acceleartor_action::kDebugPrintWindowHierarchy:
      *out = ash::AcceleratorAction::kDebugPrintWindowHierarchy;
      return true;
    case mojom_acceleartor_action::kDebugShowToast:
      *out = ash::AcceleratorAction::kDebugShowToast;
      return true;
    case mojom_acceleartor_action::kDebugSystemUiStyleViewer:
      *out = ash::AcceleratorAction::kDebugSystemUiStyleViewer;
      return true;
    case mojom_acceleartor_action::kDebugToggleDarkMode:
      *out = ash::AcceleratorAction::kDebugToggleDarkMode;
      return true;
    case mojom_acceleartor_action::kDebugToggleDynamicColor:
      *out = ash::AcceleratorAction::kDebugToggleDynamicColor;
      return true;
    case mojom_acceleartor_action::kDebugToggleGlanceables:
      *out = ash::AcceleratorAction::kDebugToggleGlanceables;
      return true;
    case mojom_acceleartor_action::kDebugTogglePowerButtonMenu:
      *out = ash::AcceleratorAction::kDebugTogglePowerButtonMenu;
      return true;
    case mojom_acceleartor_action::kDebugToggleShowDebugBorders:
      *out = ash::AcceleratorAction::kDebugToggleShowDebugBorders;
      return true;
    case mojom_acceleartor_action::kDebugToggleShowFpsCounter:
      *out = ash::AcceleratorAction::kDebugToggleShowFpsCounter;
      return true;
    case mojom_acceleartor_action::kDebugToggleShowPaintRects:
      *out = ash::AcceleratorAction::kDebugToggleShowPaintRects;
      return true;
    case mojom_acceleartor_action::kDebugToggleTouchPad:
      *out = ash::AcceleratorAction::kDebugToggleTouchPad;
      return true;
    case mojom_acceleartor_action::kDebugToggleTouchScreen:
      *out = ash::AcceleratorAction::kDebugToggleTouchScreen;
      return true;
    case mojom_acceleartor_action::kDebugToggleTabletMode:
      *out = ash::AcceleratorAction::kDebugToggleTabletMode;
      return true;
    case mojom_acceleartor_action::kDebugToggleVideoConferenceCameraTrayIcon:
      *out = ash::AcceleratorAction::kDebugToggleVideoConferenceCameraTrayIcon;
      return true;
    case mojom_acceleartor_action::kDebugToggleWallpaperMode:
      *out = ash::AcceleratorAction::kDebugToggleWallpaperMode;
      return true;
    case mojom_acceleartor_action::kDebugTriggerCrash:
      *out = ash::AcceleratorAction::kDebugTriggerCrash;
      return true;
    case mojom_acceleartor_action::kDebugToggleHudDisplay:
      *out = ash::AcceleratorAction::kDebugToggleHudDisplay;
      return true;
    case mojom_acceleartor_action::kDebugToggleVirtualTrackpad:
      *out = ash::AcceleratorAction::kDebugToggleVirtualTrackpad;
      return true;
    case mojom_acceleartor_action::kDevAddRemoveDisplay:
      *out = ash::AcceleratorAction::kDevAddRemoveDisplay;
      return true;
    case mojom_acceleartor_action::kDevToggleAppList:
      *out = ash::AcceleratorAction::kDevToggleAppList;
      return true;
    case mojom_acceleartor_action::kDevToggleUnifiedDesktop:
      *out = ash::AcceleratorAction::kDevToggleUnifiedDesktop;
      return true;
  }
  NOTREACHED_NORETURN();
}

}  // namespace mojo
