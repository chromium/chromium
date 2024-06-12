// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_SETTINGS_H_

#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"

// A class that abstracts the access to the system-level permission settings.
//
// There is a certain overlap with
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/system_permission_delegate.h
// this is intentional as explained in
// https://chromium-review.googlesource.com/c/chromium/src/+/5424111/comment/5e007f7b_c2b9ff9f
class SystemPermissionSettings : public base::SupportsUserData::Data {
 public:
  using SystemPermissionResponseCallback = base::OnceCallback<void()>;

  // base::SupportsUserData::Data:
  std::unique_ptr<base::SupportsUserData::Data> Clone() override;

  // Creates a new instance of SystemPermissionSettings that is OS-specific and
  // saves it within the profile. Should be only used when initializing the
  // Profile.
  static void Create(Profile*);

  // Gets a cached instance of SystemPermissionSettings from Profile.
  static SystemPermissionSettings* GetInstance();

  // Returns `true` if Chrome can request system-level permission. Returns
  // `false` otherwise.
  virtual bool CanPrompt(ContentSettingsType type) const = 0;

  // Check whether the system blocks the access to the specified content type /
  // permission.
  bool IsDenied(ContentSettingsType type) const;
  // Check whether the system allows the access to the specified content type /
  // permission.
  bool IsAllowed(ContentSettingsType type) const;

  // Opens the OS page where the user can change the permission settings.
  // Implementation is OS specific.
  virtual void OpenSystemSettings(content::WebContents* web_contents,
                                  ContentSettingsType type) const = 0;

  // Initiates a system permission request and invokes the provided callback
  // once the user's decision is made.
  virtual void Request(ContentSettingsType type,
                       SystemPermissionResponseCallback callback) = 0;

 private:
  virtual bool IsDeniedImpl(ContentSettingsType type) const = 0;
  virtual bool IsAllowedImpl(ContentSettingsType type) const = 0;
  static std::unique_ptr<SystemPermissionSettings> CreateImpl();
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
