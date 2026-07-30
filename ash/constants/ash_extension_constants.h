// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_EXTENSION_CONSTANTS_H_
#define ASH_CONSTANTS_ASH_EXTENSION_CONSTANTS_H_

namespace extension_misc {

// The extension id of the Assessment Assistant extension.
inline constexpr char kAssessmentAssistantExtensionId[] =
    "gndmhdcefbhlchkhipcnnbkcmicncehk";
// The extension id of the extension responsible for providing chromeos perks.
inline constexpr char kEchoExtensionId[] = "kddnkjkcjddckihglkfcickdhbmaodcn";
// The extension id of the Gnubby chrome app.
inline constexpr char kGnubbyAppId[] = "beknehfpfkghjoafdifaflglpjkojoco";
// The extension id of the new v3 Gnubby extension.
inline constexpr char kGnubbyV3ExtensionId[] =
    "lfboplenmmjcmpbkeemecobbadnmpfhi";
// The extension id of the GCSE.
inline constexpr char kGCSEExtensionId[] = "cfmgaohenjcikllcgjpepfadgbflcjof";

// The App ID of the migrated Calculator Chrome App.
inline constexpr char kCalculatorExtensionId[] =
    "joodangkbfjnajiiifokapkpmhfnpleo";

// The App ID of the migrated Google Keep Chrome App.
inline constexpr char kGoogleKeepExtensionId[] =
    "hmjkmjkepdijhoojdojkdfohbdgmmhki";

// The App ID of the migrated Google Play Books Chrome App.
inline constexpr char kGooglePlayBooksExtensionId[] =
    "mmimngoggfoobjdlefbcabngfnmieonb";

// The App ID of the migrated Google Maps Chrome App.
inline constexpr char kGoogleMapsExtensionId[] =
    "lneaknkopdijkpnocmklfnjbeapigfbh";

// The App ID of the migrated Google Play Movies Chrome App.
inline constexpr char kGooglePlayMoviesExtensionId[] =
    "gdijeikdkaembjbdobgfkoidjkpbmlkd";

// The extension id of the Desk API chrome component extension.
inline constexpr char kDeskApiExtensionId[] =
    "kflgdebkpepnpjobkdfeeipcjdahoomc";
// The extension id of the Bruschetta Security Key Forwarder extension.
inline constexpr char kBruSecurityKeyForwarderExtensionId[] =
    "lcooaekmckohjjnpaaokodoepajbnill";
// The extension id of the OneDrive FS external component extension.
inline constexpr char kODFSExtensionId[] = "gnnndjlaomemikopnjhhnoombakkkkdg";
// The extension id of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonExtensionId[] =
    "egfdjlfmgnehecnclamagfafdccgfndp";
// Path to preinstalled Accessibility Common extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kAccessibilityCommonExtensionPath[] =
    "chromeos/accessibility";
// The manifest filename of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonManifestFilename[] =
    "accessibility_common_manifest.json";
// The guest manifest filename of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonGuestManifestFilename[] =
    "accessibility_common_manifest_guest.json";
// Path to preinstalled ChromeVox screen reader extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kChromeVoxExtensionPath[] = "chromeos/accessibility";
// The manifest filename of the ChromeVox extension.
inline constexpr char kChromeVoxManifestFilename[] = "chromevox_manifest.json";
// The manifest v3 filename of the ChromeVox extension.
inline constexpr char kChromeVoxManifestV3Filename[] =
    "chromevox_manifest_v3.json";
// The guest manifest filename of the ChromeVox extension.
inline constexpr char kChromeVoxGuestManifestFilename[] =
    "chromevox_manifest_guest.json";
// The guest manifest v3 filename of the ChromeVox extension.
inline constexpr char kChromeVoxGuestManifestV3Filename[] =
    "chromevox_manifest_guest_v3.json";
// The path to the ChromeVox extension's options page.
inline constexpr char kChromeVoxOptionsPath[] =
    "/chromevox/options/options.html";
// The extension id of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsExtensionId[] =
    "jacnkoglebceckolkoapelihnglgaicd";
// Path to preinstalled Enhanced network TTS engine extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kEnhancedNetworkTtsExtensionPath[] =
    "chromeos/accessibility";
// The manifest filename of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsManifestFilename[] =
    "enhanced_network_tts_manifest.json";
// The guest manifest filename of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsGuestManifestFilename[] =
    "enhanced_network_tts_manifest_guest.json";
// The extension id of the Select-to-speak extension.
inline constexpr char kSelectToSpeakExtensionId[] =
    "klbcgckkldhdhonijdbnhhaiedfkllef";
// Path to preinstalled Select-to-speak extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kSelectToSpeakExtensionPath[] = "chromeos/accessibility";
// The manifest filename of the Select to Speak extension.
inline constexpr char kSelectToSpeakManifestFilename[] =
    "select_to_speak_manifest.json";
// The guest manifest filename of the Select to Speak extension.
inline constexpr char kSelectToSpeakGuestManifestFilename[] =
    "select_to_speak_manifest_guest.json";
// The extension id of the Switch Access extension.
inline constexpr char kSwitchAccessExtensionId[] =
    "pmehocpgjmkenlokgjfkaichfjdhpeol";
// Path to preinstalled Switch Access extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kSwitchAccessExtensionPath[] = "chromeos/accessibility";
// The manifest filename of the Switch Access extension.
inline constexpr char kSwitchAccessManifestFilename[] =
    "switch_access_manifest.json";
// The manifest v3 filename of the Switch Access extension.
inline constexpr char kSwitchAccessManifestV3Filename[] =
    "switch_access_manifest_v3.json";
// The guest manifest filename of the Switch Access extension.
inline constexpr char kSwitchAccessGuestManifestFilename[] =
    "switch_access_manifest_guest.json";
// The guest manifest v3 filename of the Switch Access extension.
inline constexpr char kSwitchAccessGuestManifestV3Filename[] =
    "switch_access_manifest_guest_v3.json";
// Name of the manifest file in an extension when a special manifest is used
// for guest mode.
inline constexpr char kGuestManifestFilename[] = "manifest_guest.json";
// The extension id of the first run dialog application.
inline constexpr char kFirstRunDialogId[] = "jdgcneonijmofocbhmijhacgchbihela";
// Path to preinstalled Google speech synthesis extension.
inline constexpr char kGoogleSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts";
inline constexpr char kGoogleSpeechSynthesisManifestV3ExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts/mv3";
// The extension id of the Google speech synthesis extension.
inline constexpr char kGoogleSpeechSynthesisExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
// The path to the Google speech synthesis extension's options page.
inline constexpr char kGoogleSpeechSynthesisOptionsPath[] = "/options.html";
// Path to preinstalled eSpeak-NG speech synthesis extension.
inline constexpr char kEspeakSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/espeak-ng";
inline constexpr char kEspeakManifestV3SpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/espeak-ng-mv3";
// The extension id of the eSpeak-NG speech synthesis extension.
inline constexpr char kEspeakSpeechSynthesisExtensionId[] =
    "dakbfdmgjiabojdgbiljlhgjbokobjpg";
// The path to the eSpeak-NG speech synthesis extension's options page.
inline constexpr char kEspeakSpeechSynthesisOptionsPath[] = "/options.html";
// The extension id of official HelpApp extension.
inline constexpr char kHelpAppExtensionId[] =
    "honijodknafkokifofgiaalefdiedpko";

}  // namespace extension_misc

#endif  // ASH_CONSTANTS_ASH_EXTENSION_CONSTANTS_H_
