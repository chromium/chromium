// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_

#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

namespace system_permission_settings {

using SystemPermissionResponseCallback = base::OnceCallback<void()>;

// Returns `true` if Chrome can request system-level permission. Returns
// `false` otherwise.
bool CanPrompt(ContentSettingsType type);

// Check whether the system blocks the access to the specified content type /
// permission.
bool IsDenied(ContentSettingsType type);

// Check whether the system allows the access to the specified content type /
// permission.
// On some platforms, both IsDenied and IsAllowed may return false for the
// same permission.
bool IsAllowed(ContentSettingsType type);

// Opens the OS page where the user can change the permission settings.
// Implementation is OS specific.
void OpenSystemSettings(content::WebContents* web_contents,
                        ContentSettingsType type);

// Initiates a system permission request and invokes the provided callback
// once the user's decision is made.
void Request(ContentSettingsType type,
             SystemPermissionResponseCallback callback);

class ScopedSettingsForTesting {
 public:
  ScopedSettingsForTesting(ContentSettingsType type, bool blocked);
  ~ScopedSettingsForTesting();

 private:
  ContentSettingsType type_;
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_
