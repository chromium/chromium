// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_PLATFORM_HANDLE_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_PLATFORM_HANDLE_H_

#include <memory>

#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

namespace system_permission_settings {

// A class that abstracts the access to the system-level permission settings.
// This class is to be implemented separately for each platform.
// A single instance is created at a startup and is accessible via
// GlobalFeatures within the BrowserProcess.
class PlatformHandle {
 public:
  virtual ~PlatformHandle() = default;

  // Returns a new instance of the platform-specific implementation.
  static std::unique_ptr<PlatformHandle> Create();

  // Returns `true` if Chrome can request system-level permission. Returns
  // `false` otherwise.
  virtual bool CanPrompt(ContentSettingsType type) = 0;

  // Returns true if the system blocks the access to the specified content type
  // permission.

  virtual bool IsDenied(ContentSettingsType type) = 0;
  // Returns true if the system blocks the access to the specified content type
  // permission.

  virtual bool IsAllowed(ContentSettingsType type) = 0;

  // Opens the OS page where the user can change the permission settings.
  // Implementation is OS specific.
  virtual void OpenSystemSettings(content::WebContents* web_contents,
                                  ContentSettingsType type) = 0;

  // Initiates a system permission request and invokes the provided callback
  // once the user's decision is made.
  virtual void Request(ContentSettingsType type,
                       SystemPermissionResponseCallback callback) = 0;

  // Creates an observation object that maintains the observation of the system
  // permission changes. As long as the object is alive, the system permission
  // changes will be passed to the observer. When the object is destroyed, the
  // observer will not receive updates any more.
  // Returns nullptr if the platform does not support observation.
  virtual std::unique_ptr<ScopedObservation> Observe(
      SystemPermissionChangedCallback observer) = 0;
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_PLATFORM_HANDLE_H_
