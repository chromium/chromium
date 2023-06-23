// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LOCAL_PASSWORDS_MIGRATION_WARNING_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LOCAL_PASSWORDS_MIGRATION_WARNING_UTIL_H_

#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/native_widget_types.h"

namespace local_password_migration {

// Shows the local password migration warning.
void ShowWarning(const gfx::NativeWindow window, Profile* profile);

// Returns whether the UPM local passwords migration warning should be
// displayed. `profile` is used to retrieve necessary services for checking
// the conditions.
bool ShouldShowWarning(Profile* profile);

}  // namespace local_password_migration

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LOCAL_PASSWORDS_MIGRATION_WARNING_UTIL_H_
