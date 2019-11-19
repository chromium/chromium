// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_ASSISTANT_UTIL_H_
#define ASH_ASSISTANT_UTIL_ASSISTANT_UTIL_H_

#include "base/component_export.h"

namespace ash {

enum class AssistantEntryPoint;
enum class AssistantVisibility;

namespace assistant {
namespace util {

// Returns true if Assistant is starting a new session, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsStartingSession(AssistantVisibility new_visibility,
                       AssistantVisibility old_visibility);

// Returns true if Assistant is finishing a session, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsFinishingSession(AssistantVisibility new_visibility);

// Returns true if the |entry_point| should start Assistant with a voice
// interaction.
// |prefer_voice| is true if user prefers voice input modality or if the device
// is in tablet mode.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsVoiceEntryPoint(AssistantEntryPoint entry_point, bool prefer_voice);

// Returns true if the |entry_point| should attempt warmer welcome.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool ShouldAttemptWarmerWelcome(AssistantEntryPoint entry_point);

COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsGoogleDevice();

COMPONENT_EXPORT(ASSISTANT_UTIL)
void OverrideIsGoogleDeviceForTesting();

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_ASSISTANT_UTIL_H_
