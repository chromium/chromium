// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/buildflags/buildflags.h"

namespace content {
class WebContents;
}

namespace permissions {
class PermissionRequestID;
}

class GURL;
class Profile;

// Chrome extensions specific portions of GeolocationPermissionContext.
class GeolocationPermissionContextExtensions {
 public:
  explicit GeolocationPermissionContextExtensions(Profile* profile);

  GeolocationPermissionContextExtensions(
      const GeolocationPermissionContextExtensions&) = delete;
  GeolocationPermissionContextExtensions& operator=(
      const GeolocationPermissionContextExtensions&) = delete;

  ~GeolocationPermissionContextExtensions();

  // Returns true if the permission request was handled. In which case,
  // |permission_set| will be set to true if the permission changed, and the
  // permission has been set to |new_permission|. Consumes |callback| if it
  // returns true while setting |permission_set| to false, otherwise |callback|
  // is not used.
  bool DecidePermission(const permissions::PermissionRequestID& request_id,
                        const GURL& requesting_frame,
                        bool user_gesture,
                        base::OnceCallback<void(ContentSetting)>* callback,
                        bool* permission_set,
                        bool* new_permission);

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  raw_ptr<Profile> profile_;
#endif
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_
