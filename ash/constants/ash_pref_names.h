// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_PREF_NAMES_H_
#define ASH_CONSTANTS_ASH_PREF_NAMES_H_

#include "base/component_export.h"

namespace ash {
namespace prefs {

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistiveInputFeatureSettings[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAssistPersonalInfoEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kESimProfilesPrefName[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEmojiSuggestionEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEmojiSuggestionEnterpriseAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioDevicesMute[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioDevicesGainPercent[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAudioDevicesVolumePercent[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioMute[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioOutputAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioVolumePercent[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kAudioDevicesState[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kEduCoexistenceId[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEduCoexistenceSecondaryAccountsInvalidationVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEduCoexistenceToSVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kEduCoexistenceToSAcceptedVersion[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kShouldSkipInlineLoginWelcomePage[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kQuirksClientLastServerCheck[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceWiFiFastTransitionEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSecondaryGoogleAccountSigninAllowed[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSamlPasswordModifiedTime[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSamlPasswordExpirationTime[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSamlPasswordChangeUrl[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSyncOobeCompleted[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLoginDisplayPasswordButtonEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSuggestedContentEnabled[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherResultEverLaunched[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kHasCameraAppMigratedToSWA[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kLauncherSearchNormalizerParameters[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kDeviceSystemWideTracingEnabled[];

}  // namespace prefs
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the Chrome OS source code
// directory migration is finished.
namespace chromeos {
namespace prefs {
using namespace ::ash::prefs;
}
}  // namespace chromeos

#endif  // ASH_CONSTANTS_ASH_PREF_NAMES_H_
