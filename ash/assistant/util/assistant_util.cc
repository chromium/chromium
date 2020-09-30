// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/assistant_util.h"

#include <string>

#include "ash/assistant/model/assistant_ui_model.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/devicetype.h"

namespace {

bool g_override_is_google_device = false;

}  // namespace

namespace ash {
namespace assistant {
namespace util {

using chromeos::assistant::AssistantEntryPoint;

bool IsStartingSession(AssistantVisibility new_visibility,
                       AssistantVisibility old_visibility) {
  return old_visibility == AssistantVisibility::kClosed &&
         new_visibility == AssistantVisibility::kVisible;
}

bool IsFinishingSession(AssistantVisibility new_visibility) {
  return new_visibility == AssistantVisibility::kClosed;
}

bool IsVoiceEntryPoint(AssistantEntryPoint entry_point, bool prefer_voice) {
  switch (entry_point) {
    case AssistantEntryPoint::kHotword:
      return true;
    case AssistantEntryPoint::kHotkey:
    case AssistantEntryPoint::kLauncherSearchBoxIcon:
    case AssistantEntryPoint::kLongPressLauncher:
      return prefer_voice;
    case AssistantEntryPoint::kUnspecified:
    case AssistantEntryPoint::kDeepLink:
    case AssistantEntryPoint::kLauncherChip:
    case AssistantEntryPoint::kLauncherSearchResult:
    case AssistantEntryPoint::kProactiveSuggestions:
    case AssistantEntryPoint::kSetup:
    case AssistantEntryPoint::kStylus:
    case AssistantEntryPoint::kBloom:
      return false;
  }
}

bool IsGoogleDevice() {
  return g_override_is_google_device || chromeos::IsGoogleBrandedDevice();
}

void OverrideIsGoogleDeviceForTesting(bool is_google_device) {
  g_override_is_google_device = is_google_device;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
