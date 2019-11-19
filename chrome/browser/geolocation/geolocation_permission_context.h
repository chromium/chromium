// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/geolocation/geolocation_permission_context_extensions.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"

namespace content {
class WebContents;
}

class PermissionRequestID;
class Profile;

class GeolocationPermissionContext  : public PermissionContextBase {
 public:
  explicit GeolocationPermissionContext(Profile* profile);
  ~GeolocationPermissionContext() override;

  // In addition to the base class flow the geolocation permission decision
  // checks that it is only code from valid iframes.
  // It also adds special logic when called through an extension.
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override;

 private:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  bool IsRestrictedToSecureOrigins() const override;

  device::mojom::GeolocationControl* GetGeolocationControl();

  // This must only be accessed from the UI thread.
  GeolocationPermissionContextExtensions extensions_context_;

  mojo::Remote<device::mojom::GeolocationControl> geolocation_control_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContext);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
