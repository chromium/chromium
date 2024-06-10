// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_

#include <memory>

#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"

// A class that abstracts the access to the system-level permission settings.
//
// There is a certain overlap with
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/system_permission_delegate.h
// this is intentional as explained in
// https://chromium-review.googlesource.com/c/chromium/src/+/5424111/comment/5e007f7b_c2b9ff9f
class SystemPermissionSettings {
 public:
  SystemPermissionSettings() = default;
  virtual ~SystemPermissionSettings() = default;
  // Creates a new instance of SystemPermissionSettings that is OS-specific.
  static std::unique_ptr<SystemPermissionSettings> Create();

  // Check whether the system blocks the access to the specified content type /
  // permission.
  bool IsPermissionDenied(ContentSettingsType type) const;

  // Opens the OS page where the user can change the permission settings.
  // Implementation is OS specific.
  virtual void OpenSystemSettings(content::WebContents* web_contents,
                                  ContentSettingsType type) const = 0;

 private:
  // Checks whether a given permission is blocked by the OS. Implementation is
  // OS specific.
  virtual bool IsPermissionDeniedImpl(ContentSettingsType type) const = 0;
};

class ScopedSystemPermissionSettingsForTesting {
 public:
  ScopedSystemPermissionSettingsForTesting(ContentSettingsType type,
                                           bool blocked);
  ~ScopedSystemPermissionSettingsForTesting();

 private:
  ContentSettingsType type_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_
