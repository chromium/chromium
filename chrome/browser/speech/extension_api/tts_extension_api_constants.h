// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace tts_extension_api_constants {

inline constexpr char kCharIndexKey[] = "charIndex";
inline constexpr char kLengthKey[] = "length";
inline constexpr char kDesiredEventTypesKey[] = "desiredEventTypes";
inline constexpr char kEnqueueKey[] = "enqueue";
inline constexpr char kErrorKey[] = "error";
inline constexpr char kErrorMessageKey[] = "errorMessage";
inline constexpr char kEventTypeKey[] = "type";
inline constexpr char kEventTypesKey[] = "eventTypes";
inline constexpr char kExtensionIdKey[] = "extensionId";
inline constexpr char kGenderKey[] = "gender";
inline constexpr char kIdKey[] = "id";
inline constexpr char kInstallStatusKey[] = "installStatus";
inline constexpr char kIsFinalEventKey[] = "isFinalEvent";
inline constexpr char kLangKey[] = "lang";
inline constexpr char kOnEventKey[] = "onEvent";
inline constexpr char kPitchKey[] = "pitch";
inline constexpr char kRateKey[] = "rate";
inline constexpr char kRemoteKey[] = "remote";
inline constexpr char kUninstallImmediatelyKey[] = "uninstallImmediately";
inline constexpr char kRequiredEventTypesKey[] = "requiredEventTypes";
inline constexpr char kSourceKey[] = "source";
inline constexpr char kSrcIdKey[] = "srcId";
inline constexpr char kVoiceNameKey[] = "voiceName";
inline constexpr char kVolumeKey[] = "volume";

inline constexpr char kSampleRateKey[] = "sampleRate";
inline constexpr char kBufferSizeKey[] = "bufferSize";
inline constexpr char kAudioBufferKey[] = "audioBuffer";
inline constexpr char kIsLastBufferKey[] = "isLastBuffer";

inline constexpr char kEventTypeCancelled[] = "cancelled";
inline constexpr char kEventTypeEnd[] = "end";
inline constexpr char kEventTypeError[] = "error";
inline constexpr char kEventTypeInterrupted[] = "interrupted";
inline constexpr char kEventTypeMarker[] = "marker";
inline constexpr char kEventTypePause[] = "pause";
inline constexpr char kEventTypeResume[] = "resume";
inline constexpr char kEventTypeSentence[] = "sentence";
inline constexpr char kEventTypeStart[] = "start";
inline constexpr char kEventTypeWord[] = "word";

// Used by TtsEngine Extension to communicate installation status of voices for
// a specific language
inline constexpr char kVoicePackStatusNotInstalled[] = "notInstalled";
inline constexpr char kVoicePackStatusInstalling[] = "installing";
inline constexpr char kVoicePackStatusInstalled[] = "installed";
inline constexpr char kVoicePackStatusFailed[] = "failed";

inline constexpr char kErrorExtensionIdMismatch[] = "Extension id mismatch.";
inline constexpr char kErrorInvalidLang[] = "Invalid lang.";
inline constexpr char kErrorInvalidPitch[] = "Invalid pitch.";
inline constexpr char kErrorInvalidRate[] = "Invalid rate.";
inline constexpr char kErrorInvalidVolume[] = "Invalid volume.";
inline constexpr char kErrorMissingPauseOrResume[] =
    "A TTS engine extension should either listen for both onPause and onResume "
    "events, or neither.";
inline constexpr char kErrorUndeclaredEventType[] =
    "Cannot send an event type that is not declared in the extension manifest.";
inline constexpr char kErrorUtteranceTooLong[] =
    "Utterance length is too long.";

}  // namespace tts_extension_api_constants.
#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
