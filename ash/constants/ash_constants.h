// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants related to ChromeOS.

#ifndef ASH_CONSTANTS_ASH_CONSTANTS_H_
#define ASH_CONSTANTS_ASH_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kDriveCacheDirname[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kNssCertDbPath[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kNssKeyDbPath[];

// The border thickness of keyboard focus for launcher items and system tray.
constexpr int kFocusBorderThickness = 2;

// The thickness of the focus bar for launcher search.
constexpr int kFocusBarThickness = 3;

// Offset added to the shelf so tray bubble bounds are in the correct display.
constexpr int kShelfDisplayOffset = 1;

constexpr int kDefaultLargeCursorSize = 64;
constexpr int kMinLargeCursorSize = 25;
constexpr int kMaxLargeCursorSize = 128;

constexpr int kDefaultCaretBlinkIntervalMs = 500;

constexpr SkColor kDefaultCursorColor = SK_ColorBLACK;

// Default notification flash color is yellow.
constexpr SkColor kDefaultFlashNotificationsColor = 0xffff00;

// These device types are a subset of ui::InputDeviceType. These strings are
// also used in Switch Access webui.
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwitchAccessInternalDevice[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwitchAccessUsbDevice[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSwitchAccessBluetoothDevice[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwitchAccessUnknownDevice[];

// The string that represents the current time. Only used in pixel tests.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kFakeNowTimeStringInPixelTest[];

// The default delay before Switch Access automatically moves to the next
// element on the page that is interesting, based on the Switch Access
// predicates. This value is mostly overridden by the setup guide's default
// value.
constexpr base::TimeDelta kDefaultSwitchAccessAutoScanSpeed =
    base::Milliseconds(1800);

// The default speed in dips per second that the gliding point scan cursor
// in switch access moves across the screen.
constexpr int kDefaultSwitchAccessPointScanSpeedDipsPerSecond = 50;

// The default wait time between last mouse movement and sending autoclick.
constexpr int kDefaultAutoclickDelayMs = 1000;

// The default threshold of mouse movement, measured in DIP, that will initiate
// a new autoclick.
constexpr int kDefaultAutoclickMovementThreshold = 20;

// Whether keyboard auto repeat is enabled by default.
constexpr bool kDefaultKeyAutoRepeatEnabled = true;

// Whether dark mode is enabled by default.
constexpr bool kDefaultDarkModeEnabled = false;

// Maximum number of times that dark/light mode educational nudge can be shown.
constexpr int kDarkLightModeNudgeMaxShownCount = 3;

// The default delay before a held keypress will start to auto repeat.
constexpr base::TimeDelta kDefaultKeyAutoRepeatDelay = base::Milliseconds(500);

// The default interval between auto-repeated key events.
constexpr base::TimeDelta kDefaultKeyAutoRepeatInterval =
    base::Milliseconds(50);

// Constants for notification.
const char kPrivacyIndicatorsNotificationIdPrefix[] = "privacy-indicators";
const char kPrivacyIndicatorsNotifierId[] = "ash.privacy-indicators";

// The default value for audio strategy in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxAudioStrategy[];

// The default value for the braille table in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxBrailleTable[];

// The default value for the 6-dot braille table in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxBrailleTable6[];

// The default value for the 8-dot braille table in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxBrailleTable8[];

// The default value for the braille table type in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxBrailleTableType[];

// The default value for the capital strategy in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxCapitalStrategy[];

// The default value for the capital strategy backup pref in ChromeVox, used
// on the settings page for saving a user's preference when they toggle
// |usePitchChanges|.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxCapitalStrategyBackup[];

// The default value for number reading style in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxNumberReadingStyle[];

// The default value for the preferred braille display address in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char
    kDefaultAccessibilityChromeVoxPreferredBrailleDisplayAddress[];

enum ChromeVoxPunctuationEcho {
  kPunctuationEchoNone = 0,
  kPunctuationEchoSome = 1,
  kPunctuationEchoAll = 2
};

// The default value for punctuation echo in ChromeVox.
constexpr int kDefaultAccessibilityChromeVoxPunctuationEcho =
    ChromeVoxPunctuationEcho::kPunctuationEchoSome;

// The default value for the number of virtual braille columns in ChromeVox.
constexpr int kDefaultAccessibilityChromeVoxVirtualBrailleColumns = 40;

// The default value for the number of virtual braille rows in ChromeVox.
constexpr int kDefaultAccessibilityChromeVoxVirtualBrailleRows = 1;

// The default value for voice name in ChromeVox.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilityChromeVoxVoiceName[];

// Whether the enhanced network voices feature in Select-to-speak is allowed by
// default.
constexpr bool
    kDefaultAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed = true;

// Whether Select-to-speak shades the background contents that aren't being read
// by default.
constexpr bool kDefaultAccessibilitySelectToSpeakBackgroundShading = false;

// Whether enhanced network TTS voices are enabled for Select-to-speak by
// default.
constexpr bool kDefaultAccessibilitySelectToSpeakEnhancedNetworkVoices = false;

// The default preferred enhanced voice for Select-to-speak.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilitySelectToSpeakEnhancedVoiceName[];

// Whether the initial popup authorizing enhanced network voices for
// Select-to-speak has been shown to the user by default.
constexpr bool kDefaultAccessibilitySelectToSpeakEnhancedVoicesDialogShown =
    false;

// The default word highlighting color for Select-to-speak.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilitySelectToSpeakHighlightColor[];

// Whether Select-to-speak shows navigation controls by default.
constexpr bool kDefaultAccessibilitySelectToSpeakNavigationControls = true;

// The default preferred voice for Select-to-speak.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDefaultAccessibilitySelectToSpeakVoiceName[];

// Whether Select-to-speak enables automatic voice switching between different
// languages by default.
constexpr bool kDefaultAccessibilitySelectToSpeakVoiceSwitching = false;

// Whether Select-to-speak highlights each word as it is read by default.
constexpr bool kDefaultAccessibilitySelectToSpeakWordHighlight = true;

// How much to scale cursor speed in various directions.
constexpr int kDefaultFaceGazeCursorSpeed = 10;

// How much FaceGaze should smooth recent cursor movements.
constexpr int kDefaultFaceGazeCursorSmoothing = 7;

// Whether to use cursor acceleration.
constexpr bool kDefaultFaceGazeCursorUseAcceleration = true;

// How much FaceGaze should threshold velocity, e.g. to implement deadzone.
constexpr int kDefaultFaceGazeVelocityThreshold = 9;

}  // namespace ash

#endif  // ASH_CONSTANTS_ASH_CONSTANTS_H_
