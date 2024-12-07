// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

Profile* LastActiveGlicProfile() {
  // TODO(https://crbug.com/379165457): Use the last profile opened with glic/
  // the pinned glic profile before using the last active profile.
  return ProfileManager::GetLastUsedProfileAllowedByPolicy();
}
