// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_H_

#include "base/functional/callback.h"
#include "chrome/browser/geolocation/geolocation_permission_context_extensions.h"
#include "components/permissions/contexts/geolocation_permission_context.h"

namespace content {
class WebContents;
}

namespace permissions {
class PermissionRequestID;
}

class GeolocationPermissionContextDelegate
    : public permissions::GeolocationPermissionContext::Delegate {
 public:
  explicit GeolocationPermissionContextDelegate(
      content::BrowserContext* browser_context);

  GeolocationPermissionContextDelegate(
      const GeolocationPermissionContextDelegate&) = delete;
  GeolocationPermissionContextDelegate& operator=(
      const GeolocationPermissionContextDelegate&) = delete;

  ~GeolocationPermissionContextDelegate() override;

  // In addition to the base class flow the geolocation permission decision
  // checks that it is only code from valid iframes.
  // It also adds special logic when called through an extension.
  bool DecidePermission(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback* callback,
      permissions::GeolocationPermissionContext* context) override;

 private:
  // This must only be accessed from the UI thread.
  GeolocationPermissionContextExtensions extensions_context_;
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_H_
