// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_MODE_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_MODE_H_

namespace autofill {

// This describes the different modes for the save/update address profile
// prompt. This is used to tailor the UI of the prompt in Android. A java
// IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill
enum class SaveUpdateAddressProfilePromptMode {
  // The prompt is for creating a new address profile from settings.
  kCreateNewProfile,
  // The prompt is for saving a new profile.
  kSaveNewProfile,
  // The prompt is for updating an existing profile.
  kUpdateProfile,
  // The prompt is for migrating a local profile to the user's Google Account.
  kMigrateProfile
};
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_MODE_H_
