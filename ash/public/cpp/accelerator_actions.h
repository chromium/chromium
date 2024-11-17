// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_ACTIONS_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_ACTIONS_H_

#include "ash/public/cpp/ash_public_export.h"

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
#define ACCELERATOR_ACTIONS                                            \
  ACCELERATOR_ACTION_ENTRY(BrightnessDown)                             \
  ACCELERATOR_ACTION_ENTRY(BrightnessUp)                               \
  ACCELERATOR_ACTION_ENTRY(CycleBackwardMru)                           \
  ACCELERATOR_ACTION_ENTRY(CycleForwardMru)                            \
  ACCELERATOR_ACTION_ENTRY(CycleSameAppWindowsBackward)                \
  ACCELERATOR_ACTION_ENTRY(CycleSameAppWindowsForward)                 \
  ACCELERATOR_ACTION_ENTRY(DesksActivateDeskLeft)                      \
  ACCELERATOR_ACTION_ENTRY(DesksActivateDeskRight)                     \
  ACCELERATOR_ACTION_ENTRY(DesksMoveActiveItemLeft)                    \
  ACCELERATOR_ACTION_ENTRY(DesksMoveActiveItemRight)                   \
  ACCELERATOR_ACTION_ENTRY(DesksNewDesk)                               \
  ACCELERATOR_ACTION_ENTRY(DesksRemoveCurrentDesk)                     \
  ACCELERATOR_ACTION_ENTRY(DesksActivate0)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate1)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate2)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate3)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate4)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate5)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate6)                             \
  ACCELERATOR_ACTION_ENTRY(DesksActivate7)                             \
  ACCELERATOR_ACTION_ENTRY(DesksToggleAssignToAllDesks)                \
  ACCELERATOR_ACTION_ENTRY(DisableCapsLock)                            \
  ACCELERATOR_ACTION_ENTRY(EnableOrToggleDictation)                    \
  ACCELERATOR_ACTION_ENTRY(Exit)                                       \
  ACCELERATOR_ACTION_ENTRY(FocusCameraPreview)                         \
  ACCELERATOR_ACTION_ENTRY(FocusNextPane)                              \
  ACCELERATOR_ACTION_ENTRY(FocusPreviousPane)                          \
  ACCELERATOR_ACTION_ENTRY(FocusShelf)                                 \
  ACCELERATOR_ACTION_ENTRY(FocusPip)                                   \
  ACCELERATOR_ACTION_ENTRY(KeyboardBacklightToggle)                    \
  ACCELERATOR_ACTION_ENTRY(KeyboardBrightnessDown)                     \
  ACCELERATOR_ACTION_ENTRY(KeyboardBrightnessUp)                       \
  ACCELERATOR_ACTION_ENTRY(LaunchApp0)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp1)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp2)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp3)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp4)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp5)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp6)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchApp7)                                 \
  ACCELERATOR_ACTION_ENTRY(LaunchLastApp)                              \
  ACCELERATOR_ACTION_ENTRY(LockPressed)                                \
  ACCELERATOR_ACTION_ENTRY(LockReleased)                               \
  ACCELERATOR_ACTION_ENTRY(LockScreen)                                 \
  ACCELERATOR_ACTION_ENTRY(MagnifierZoomIn)                            \
  ACCELERATOR_ACTION_ENTRY(MagnifierZoomOut)                           \
  ACCELERATOR_ACTION_ENTRY(MediaFastForward)                           \
  ACCELERATOR_ACTION_ENTRY(MediaNextTrack)                             \
  ACCELERATOR_ACTION_ENTRY(MediaPause)                                 \
  ACCELERATOR_ACTION_ENTRY(MediaPlay)                                  \
  ACCELERATOR_ACTION_ENTRY(MediaPlayPause)                             \
  ACCELERATOR_ACTION_ENTRY(MediaPrevTrack)                             \
  ACCELERATOR_ACTION_ENTRY(MediaRewind)                                \
  ACCELERATOR_ACTION_ENTRY(MediaStop)                                  \
  ACCELERATOR_ACTION_ENTRY(MicrophoneMuteToggle)                       \
  ACCELERATOR_ACTION_ENTRY(MoveActiveWindowBetweenDisplays)            \
  ACCELERATOR_ACTION_ENTRY(NewIncognitoWindow)                         \
  ACCELERATOR_ACTION_ENTRY(NewTab)                                     \
  ACCELERATOR_ACTION_ENTRY(NewWindow)                                  \
  ACCELERATOR_ACTION_ENTRY(OpenCalculator)                             \
  ACCELERATOR_ACTION_ENTRY(OpenCrosh)                                  \
  ACCELERATOR_ACTION_ENTRY(OpenDiagnostics)                            \
  ACCELERATOR_ACTION_ENTRY(OpenFeedbackPage)                           \
  ACCELERATOR_ACTION_ENTRY(OpenFileManager)                            \
  ACCELERATOR_ACTION_ENTRY(OpenGetHelp)                                \
  /* Similar to kToggleClipboardHistory but is used to paste plain*/   \
  /* text only when clipboard history menu is already open. */         \
  ACCELERATOR_ACTION_ENTRY(PasteClipboardHistoryPlainText)             \
  ACCELERATOR_ACTION_ENTRY(PowerPressed)                               \
  ACCELERATOR_ACTION_ENTRY(PowerReleased)                              \
  ACCELERATOR_ACTION_ENTRY(PrintUiHierarchies)                         \
  ACCELERATOR_ACTION_ENTRY(PrivacyScreenToggle)                        \
  ACCELERATOR_ACTION_ENTRY(RestoreTab)                                 \
  ACCELERATOR_ACTION_ENTRY(RotateScreen)                               \
  ACCELERATOR_ACTION_ENTRY(RotateWindow)                               \
  ACCELERATOR_ACTION_ENTRY(ScaleUiDown)                                \
  ACCELERATOR_ACTION_ENTRY(ScaleUiReset)                               \
  ACCELERATOR_ACTION_ENTRY(ScaleUiUp)                                  \
  ACCELERATOR_ACTION_ENTRY(ShowEmojiPicker)                            \
  ACCELERATOR_ACTION_ENTRY(ToggleImeMenuBubble)                        \
  ACCELERATOR_ACTION_ENTRY(ShowShortcutViewer)                         \
  ACCELERATOR_ACTION_ENTRY(ShowTaskManager)                            \
  ACCELERATOR_ACTION_ENTRY(StartAssistant)                             \
  ACCELERATOR_ACTION_ENTRY(StopScreenRecording)                        \
  ACCELERATOR_ACTION_ENTRY(Suspend)                                    \
  ACCELERATOR_ACTION_ENTRY(SwapPrimaryDisplay)                         \
  /* Switch to another IME depending on the accelerator. */            \
  ACCELERATOR_ACTION_ENTRY(SwitchIme)                                  \
  ACCELERATOR_ACTION_ENTRY(SwitchToLastUsedIme)                        \
  ACCELERATOR_ACTION_ENTRY(SwitchToNextIme)                            \
  ACCELERATOR_ACTION_ENTRY(SwitchToNextUser)                           \
  ACCELERATOR_ACTION_ENTRY(SwitchToPreviousUser)                       \
  ACCELERATOR_ACTION_ENTRY(TakePartialScreenshot)                      \
  ACCELERATOR_ACTION_ENTRY(TakeScreenshot)                             \
  ACCELERATOR_ACTION_ENTRY(TakeWindowScreenshot)                       \
  ACCELERATOR_ACTION_ENTRY(ToggleAppList)                              \
  ACCELERATOR_ACTION_ENTRY(ToggleCalendar)                             \
  ACCELERATOR_ACTION_ENTRY(ToggleCapsLock)                             \
  ACCELERATOR_ACTION_ENTRY(ToggleClipboardHistory)                     \
  ACCELERATOR_ACTION_ENTRY(ToggleDockedMagnifier)                      \
  ACCELERATOR_ACTION_ENTRY(ToggleFloating)                             \
  ACCELERATOR_ACTION_ENTRY(ToggleFullscreen)                           \
  ACCELERATOR_ACTION_ENTRY(ToggleFullscreenMagnifier)                  \
  ACCELERATOR_ACTION_ENTRY(ToggleGameDashboard)                        \
  ACCELERATOR_ACTION_ENTRY(ToggleHighContrast)                         \
  ACCELERATOR_ACTION_ENTRY(ToggleMaximized)                            \
  ACCELERATOR_ACTION_ENTRY(ToggleMessageCenterBubble)                  \
  ACCELERATOR_ACTION_ENTRY(ToggleMirrorMode)                           \
  ACCELERATOR_ACTION_ENTRY(ToggleMultitaskMenu)                        \
  ACCELERATOR_ACTION_ENTRY(ToggleOverview)                             \
  ACCELERATOR_ACTION_ENTRY(ToggleProjectorMarker)                      \
  ACCELERATOR_ACTION_ENTRY(ToggleResizeLockMenu)                       \
  ACCELERATOR_ACTION_ENTRY(CreateSnapGroup)                            \
  ACCELERATOR_ACTION_ENTRY(ToggleSnapGroupWindowsMinimizeAndRestore)   \
  ACCELERATOR_ACTION_ENTRY(ToggleSpokenFeedback)                       \
  ACCELERATOR_ACTION_ENTRY(ToggleStylusTools)                          \
  ACCELERATOR_ACTION_ENTRY(ToggleSystemTrayBubble)                     \
  ACCELERATOR_ACTION_ENTRY(ToggleWifi)                                 \
  ACCELERATOR_ACTION_ENTRY(TouchHudClear)                              \
  ACCELERATOR_ACTION_ENTRY(TouchHudModeChange)                         \
  ACCELERATOR_ACTION_ENTRY(TouchFingerprintSensor1)                    \
  ACCELERATOR_ACTION_ENTRY(TouchFingerprintSensor2)                    \
  ACCELERATOR_ACTION_ENTRY(TouchFingerprintSensor3)                    \
  ACCELERATOR_ACTION_ENTRY(Unpin)                                      \
  ACCELERATOR_ACTION_ENTRY(VolumeDown)                                 \
  ACCELERATOR_ACTION_ENTRY(VolumeMute)                                 \
  ACCELERATOR_ACTION_ENTRY(VolumeUp)                                   \
  ACCELERATOR_ACTION_ENTRY(WindowCycleSnapLeft)                        \
  ACCELERATOR_ACTION_ENTRY(WindowCycleSnapRight)                       \
  ACCELERATOR_ACTION_ENTRY(WindowMinimize)                             \
  ACCELERATOR_ACTION_ENTRY(MinimizeTopWindowOnBack)                    \
  ACCELERATOR_ACTION_ENTRY(VolumeMuteToggle)                           \
  ACCELERATOR_ACTION_ENTRY(TogglePicker)                               \
  ACCELERATOR_ACTION_ENTRY(AccessibilityAction)                        \
  ACCELERATOR_ACTION_ENTRY(EnableSelectToSpeak)                        \
  ACCELERATOR_ACTION_ENTRY(TilingWindowResizeLeft)                     \
  ACCELERATOR_ACTION_ENTRY(TilingWindowResizeRight)                    \
  ACCELERATOR_ACTION_ENTRY(TilingWindowResizeUp)                       \
  ACCELERATOR_ACTION_ENTRY(TilingWindowResizeDown)                     \
  ACCELERATOR_ACTION_ENTRY(ToggleMouseKeys)                            \
  ACCELERATOR_ACTION_ENTRY(ResizePipWindow)                            \
  /* Debug actions are kept at an offset.*/                            \
  /* This offset should be kept consistent with the enum*/             \
  /* `AcceleratorAction` in*/                                          \
  /* tools/metrics/histograms/metadata/chromeos/enums.xml */           \
  ACCELERATOR_ACTION_ENTRY_FIXED_VALUE(DebugClearUseKMeansPref, 9000)  \
  ACCELERATOR_ACTION_ENTRY(DebugKeyboardBacklightToggle)               \
  ACCELERATOR_ACTION_ENTRY(DebugMicrophoneMuteToggle)                  \
  ACCELERATOR_ACTION_ENTRY(DebugPrintLayerHierarchy)                   \
  ACCELERATOR_ACTION_ENTRY(DebugPrintViewHierarchy)                    \
  ACCELERATOR_ACTION_ENTRY(DebugPrintWindowHierarchy)                  \
  ACCELERATOR_ACTION_ENTRY(DebugShowInformedRestore)                   \
  ACCELERATOR_ACTION_ENTRY(DebugShowToast)                             \
  ACCELERATOR_ACTION_ENTRY(DebugShowSystemNudge)                       \
  ACCELERATOR_ACTION_ENTRY(DebugSystemUiStyleViewer)                   \
  ACCELERATOR_ACTION_ENTRY(DebugToggleDarkMode)                        \
  ACCELERATOR_ACTION_ENTRY(DebugToggleDynamicColor)                    \
  ACCELERATOR_ACTION_ENTRY(DebugTogglePowerButtonMenu)                 \
  ACCELERATOR_ACTION_ENTRY(DebugToggleShowDebugBorders)                \
  ACCELERATOR_ACTION_ENTRY(DebugToggleShowFpsCounter)                  \
  ACCELERATOR_ACTION_ENTRY(DebugToggleShowPaintRects)                  \
  ACCELERATOR_ACTION_ENTRY(DebugToggleTouchPad)                        \
  ACCELERATOR_ACTION_ENTRY(DebugToggleTouchScreen)                     \
  ACCELERATOR_ACTION_ENTRY(DebugToggleTabletMode)                      \
  ACCELERATOR_ACTION_ENTRY(DebugToggleVideoConferenceCameraTrayIcon)   \
  ACCELERATOR_ACTION_ENTRY(DebugToggleWallpaperMode)                   \
  /* Intentionally crash the ash process. */                           \
  ACCELERATOR_ACTION_ENTRY(DebugTriggerCrash)                          \
  ACCELERATOR_ACTION_ENTRY(DebugToggleHudDisplay)                      \
  ACCELERATOR_ACTION_ENTRY(DebugToggleVirtualTrackpad)                 \
  ACCELERATOR_ACTION_ENTRY(DevAddRemoveDisplay)                        \
  /* Different than kToggleAppList to ignore search-as-modifier-key */ \
  /* rules for enabling the accelerator. */                            \
  ACCELERATOR_ACTION_ENTRY(DevToggleAppList)                           \
  ACCELERATOR_ACTION_ENTRY(DevToggleUnifiedDesktop)                    \
  ACCELERATOR_ACTION_ENTRY(DebugToggleFocusModeState)                  \
  ACCELERATOR_ACTION_ENTRY(DebugStartSunfishSession)                   \
  // LINT.ThenChange(//ash/public/mojom/accelerator_actions.mojom)

enum AcceleratorAction {
#define ACCELERATOR_ACTION_ENTRY(action) k##action,
#define ACCELERATOR_ACTION_ENTRY_FIXED_VALUE(action, value) k##action = value,
  ACCELERATOR_ACTIONS
#undef ACCELERATOR_ACTION_ENTRY
#undef ACCELERATOR_ACTION_ENTRY_FIXED_VALUE
};

ASH_PUBLIC_EXPORT const char* GetAcceleratorActionName(
    AcceleratorAction action);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_ACTIONS_H_
