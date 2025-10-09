// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/assistant/assistant_util.h"

#include <string>

#include "ash/constants/devicetype.h"
#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/icu/source/common/unicode/locid.h"


namespace assistant {

::ash::assistant::AssistantAllowedState IsAssistantAllowedForProfile(
    const Profile* profile) {
  return ::ash::assistant::AssistantAllowedState::DISALLOWED_BY_NEW_ENTRY_POINT;
}

}  // namespace assistant
