// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ASSISTANT_ASSISTANT_UTIL_H_
#define CHROME_BROWSER_ASH_ASSISTANT_ASSISTANT_UTIL_H_

#include "ash/public/cpp/assistant/assistant_state_base.h"

class Profile;

namespace assistant {

// Returns whether Google Assistant feature is allowed for given |profile|.
ash::assistant::AssistantAllowedState IsAssistantAllowedForProfile(
    const Profile* profile);

void OverrideIsGoogleDeviceForTesting(bool is_google_device);

}  // namespace assistant

#endif  // CHROME_BROWSER_ASH_ASSISTANT_ASSISTANT_UTIL_H_
