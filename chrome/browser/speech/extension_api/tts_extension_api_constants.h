// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_


namespace tts_extension_api_constants {

extern const char kCharIndexKey[];
extern const char kLengthKey[];
extern const char kDesiredEventTypesKey[];
extern const char kEnqueueKey[];
extern const char kErrorMessageKey[];
extern const char kEventTypeKey[];
extern const char kEventTypesKey[];
extern const char kExtensionIdKey[];
extern const char kGenderKey[];
extern const char kIsFinalEventKey[];
extern const char kLangKey[];
extern const char kOnEventKey[];
extern const char kPitchKey[];
extern const char kRateKey[];
extern const char kRemoteKey[];
extern const char kRequiredEventTypesKey[];
extern const char kSrcIdKey[];
extern const char kVoiceNameKey[];
extern const char kVolumeKey[];

extern const char kSampleRateKey[];
extern const char kBufferSizeKey[];
extern const char kAudioBufferKey[];
extern const char kIsLastBufferKey[];

extern const char kEventTypeCancelled[];
extern const char kEventTypeEnd[];
extern const char kEventTypeError[];
extern const char kEventTypeInterrupted[];
extern const char kEventTypeMarker[];
extern const char kEventTypePause[];
extern const char kEventTypeResume[];
extern const char kEventTypeSentence[];
extern const char kEventTypeStart[];
extern const char kEventTypeWord[];

extern const char kErrorExtensionIdMismatch[];
extern const char kErrorInvalidLang[];
extern const char kErrorInvalidPitch[];
extern const char kErrorInvalidRate[];
extern const char kErrorInvalidVolume[];
extern const char kErrorMissingPauseOrResume[];
extern const char kErrorUndeclaredEventType[];
extern const char kErrorUtteranceTooLong[];

}  // namespace tts_extension_api_constants.
#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
