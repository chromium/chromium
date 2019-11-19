// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request.h"
#include "build/build_config.h"

PermissionRequest::PermissionRequest() {}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionRequestGestureType::UNKNOWN;
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  return ContentSettingsType::DEFAULT;
}

#if defined(OS_ANDROID)
base::string16 PermissionRequest::GetQuietTitleText() const {
  return GetTitleText();
}

base::string16 PermissionRequest::GetQuietMessageText() const {
  return GetMessageText();
}
#endif
