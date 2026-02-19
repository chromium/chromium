// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_prompt_decision.h"
#include "content/public/browser/permission_result.h"
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

  struct Decision {
    bool permission_set;
    permissions::PermissionPromptDecision decision =
        permissions::PermissionPromptDecision{
            .overall_decision = PermissionDecision::kNone,
            .prompt_options = std::monostate(),
            .is_final = true,
        };
  };

  // Potentially handles a permission request. If it does, it returns a Decision
  // where |permission_set| is set to true if the permission changed, and if so
  // |decision| contains the decided |new_permission|. Consumes |callback| if it
  // returns a Decision where |permission_set| false, otherwise |callback| is
  // not used.
  std::optional<Decision> DecidePermission(
      const permissions::PermissionRequestID& request_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::OnceCallback<void(content::PermissionResult)>* callback);

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  raw_ptr<Profile> profile_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_EXTENSIONS_H_
