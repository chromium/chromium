// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_ACTIONS_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_ACTIONS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/containers/flat_set.h"

namespace ash {

// IMPORTANT PLEASE READ.
// Please ensure that the order of these enums are stable. If adding a new
// accelerator action, please put at the end but before DEBUG-related
// accelerator actions. You will also need to update `AcceleratorAction` in
// tools/metrics/histograms/metadata/chromeos/enums.xml
// Please keep the ActionName in sync with the ActionName under
// <histogram name="Ash.Accelerators.Actions.{ActionName}" in this file
// tools/metrics/histograms/metadata/ash/histograms.xml
// AND
// ash/public/mojom/accelerator_actions.mojom.
// AND
// `AcceleratorAction` enum in
// tools/metrics/histograms/metadata/chromeos/enums.xml.
//
// Accelerator actions are defined as:
//  ACCELERATOR_ACTION_ENTRY(NewAction) \
//
// The added enum member is kNewAction. Its string name is NewAction.
//
// LINT.IfChange
enum AcceleratorAction {
  kBrightnessDown,
  kBrightnessUp,
  kCycleBackwardMru,
  kCycleForwardMru,
  kCycleSameAppWindowsBackward,
  kCycleSameAppWindowsForward,
  kDesksActivateDeskLeft,
  kDesksActivateDeskRight,
  kDesksMoveActiveItemLeft,
  kDesksMoveActiveItemRight,
  kDesksNewDesk,
  kDesksRemoveCurrentDesk,
  kDesksActivate0,
  kDesksActivate1,
  kDesksActivate2,
  kDesksActivate3,
  kDesksActivate4,
  kDesksActivate5,
  kDesksActivate6,
  kDesksActivate7,
  kDesksToggleAssignToAllDesks,
  kDisableCapsLock,
  kEnableOrToggleDictation,
  kExit,
  kFocusCameraPreview,
  kFocusNextPane,
  kFocusPreviousPane,
  kFocusShelf,
  kFocusPip,
  kKeyboardBacklightToggle,
  kKeyboardBrightnessDown,
  kKeyboardBrightnessUp,
  kLaunchApp0,
  kLaunchApp1,
  kLaunchApp2,
  kLaunchApp3,
  kLaunchApp4,
  kLaunchApp5,
  kLaunchApp6,
  kLaunchApp7,
  kLaunchLastApp,
  kLockPressed,
  kLockReleased,
  kLockScreen,
  kMagnifierZoomIn,
  kMagnifierZoomOut,
  kMediaFastForward,
  kMediaNextTrack,
  kMediaPause,
  kMediaPlay,
  kMediaPlayPause,
  kMediaPrevTrack,
  kMediaRewind,
  kMediaStop,
  kMicrophoneMuteToggle,
  kMoveActiveWindowBetweenDisplays,
  kNewIncognitoWindow,
  kNewTab,
  kNewWindow,
  kOpenCalculator,
  kOpenCrosh,
  kOpenDiagnostics,
  kOpenFeedbackPage,
  kOpenFileManager,
  kOpenGetHelp,
  // Similar to kToggleClipboardHistory but is used to paste plain
  // text only when clipboard history menu is already open.
  kPasteClipboardHistoryPlainText,
  kPowerPressed,
  kPowerReleased,
  kPrintUiHierarchies,
  kPrivacyScreenToggle,
  kRestoreTab,
  kRotateScreen,
  kRotateWindow,
  kScaleUiDown,
  kScaleUiReset,
  kScaleUiUp,
  kShowEmojiPicker,
  kToggleImeMenuBubble,
  kShowShortcutViewer,
  kShowTaskManager,
  kStartAssistant,
  kStopScreenRecording,
  kSuspend,
  kSwapPrimaryDisplay,
  // Switch to another IME depending on the accelerator.
  kSwitchIme,
  kSwitchToLastUsedIme,
  kSwitchToNextIme,
  kSwitchToNextUser,
  kSwitchToPreviousUser,
  kTakePartialScreenshot,
  kTakeScreenshot,
  kTakeWindowScreenshot,
  kToggleAppList,
  kToggleCalendar,
  kToggleCapsLock,
  kToggleClipboardHistory,
  kToggleDockedMagnifier,
  kToggleFloating,
  kToggleFullscreen,
  kToggleFullscreenMagnifier,
  kToggleGameDashboard,
  kToggleHighContrast,
  kToggleMaximized,
  kToggleMessageCenterBubble,
  kToggleMirrorMode,
  kToggleMultitaskMenu,
  kToggleOverview,
  kToggleProjectorMarker,
  kToggleResizeLockMenu,
  kToggleSnapGroup,
  kToggleSnapGroupWindowsMinimizeAndRestore,
  kToggleSpokenFeedback,
  kToggleStylusTools,
  kToggleSystemTrayBubble,
  kToggleWifi,
  kTouchHudClear,
  kTouchHudModeChange,
  kTouchFingerprintSensor1,
  kTouchFingerprintSensor2,
  kTouchFingerprintSensor3,
  kUnpin,
  kVolumeDown,
  kVolumeMute,
  kVolumeUp,
  kWindowCycleSnapLeft,
  kWindowCycleSnapRight,
  kWindowMinimize,
  kMinimizeTopWindowOnBack,
  kVolumeMuteToggle,
  kToggleQuickInsert,
  kAccessibilityAction,
  kEnableSelectToSpeak,
  kTilingWindowResizeLeft,
  kTilingWindowResizeRight,
  kTilingWindowResizeUp,
  kTilingWindowResizeDown,
  kToggleMouseKeys,
  kResizePipWindow,
  kToggleGeminiApp,
  kToggleDoNotDisturb,
  kToggleCameraAllowed,
  kStartSunfishSession,
  // Debug actions are kept at an offset.  This offset should be kept consistent
  // with the enum `AcceleratorAction` in
  // tools/metrics/histograms/metadata/chromeos/enums.xml
  kDebugClearUseKMeansPref = 9000,
  kDebugKeyboardBacklightToggle,
  kDebugMicrophoneMuteToggle,
  kDebugPrintLayerHierarchy,
  kDebugPrintViewHierarchy,
  kDebugPrintWindowHierarchy,
  kDebugShowInformedRestore,
  kDebugShowToast,
  kDebugShowSystemNudge,
  kDebugSystemUiStyleViewer,
  kDebugToggleDarkMode,
  kDebugToggleDynamicColor,
  kDebugTogglePowerButtonMenu,
  kDebugToggleShowDebugBorders,
  kDebugToggleShowFpsCounter,
  kDebugToggleShowPaintRects,
  kDebugToggleTouchPad,
  kDebugToggleTouchScreen,
  kDebugToggleTabletMode,
  kDebugToggleVideoConferenceCameraTrayIcon,
  kDebugToggleWallpaperMode,
  // Intentionally crash the ash process.
  kDebugTriggerCrash,
  kDebugToggleHudDisplay,
  kDebugToggleVirtualTrackpad,
  kDevAddRemoveDisplay,
  // Different than kToggleAppList to ignore search-as-modifier-key
  // rules for enabling the accelerator.
  kDevToggleAppList,
  kDevToggleUnifiedDesktop,
  kDebugToggleFocusModeState,
  kDebugStartSunfishSession,
  kDebugShowTestWindow,
};
// LINT.ThenChange(//ash/public/mojom/accelerator_actions.mojom)

ASH_PUBLIC_EXPORT const char* GetAcceleratorActionName(
    AcceleratorAction action);

ASH_PUBLIC_EXPORT base::flat_set<AcceleratorAction>
GetAcceleratorActionsForTest();

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_ACTIONS_H_
