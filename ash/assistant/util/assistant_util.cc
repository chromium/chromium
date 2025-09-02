// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/assistant_util.h"

#include <string>

#include "ash/constants/devicetype.h"
#include "base/strings/string_util.h"

namespace {

bool g_override_is_google_device = false;

}  // namespace

namespace ash {
namespace assistant {
namespace util {

bool IsStartingSession(AssistantVisibility new_visibility,
                       AssistantVisibility old_visibility) {
  return false;
}

bool IsFinishingSession(AssistantVisibility new_visibility) {
  return false;
}

bool IsVoiceEntryPoint(AssistantEntryPoint entry_point, bool prefer_voice) {
  return false;
}

bool IsGoogleDevice() {
  return g_override_is_google_device || IsGoogleBrandedDevice();
}

void OverrideIsGoogleDeviceForTesting(bool is_google_device) {
  g_override_is_google_device = is_google_device;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
