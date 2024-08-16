// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/chrome_camera_pan_tilt_zoom_permission_context_delegate.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/compiler_specific.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
#include "extensions/browser/extension_registry.h"         // nogncheck
#include "extensions/browser/extensions_browser_client.h"  // nogncheck
#include "extensions/browser/kiosk/kiosk_delegate.h"       // nogncheck
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)

ChromeCameraPanTiltZoomPermissionContextDelegate::
    ChromeCameraPanTiltZoomPermissionContextDelegate(
        content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ChromeCameraPanTiltZoomPermissionContextDelegate::
    ~ChromeCameraPanTiltZoomPermissionContextDelegate() = default;

bool ChromeCameraPanTiltZoomPermissionContextDelegate::
    GetPermissionStatusInternal(const GURL& requesting_origin,
                                const GURL& embedding_origin,
                                ContentSetting* content_setting_result) {
#if BUILDFLAG(IS_ANDROID)
  // The PTZ permission is automatically granted on Android. It is safe to do so
  // because pan and tilt are not supported on Android.
  *content_setting_result = CONTENT_SETTING_ALLOW;
  return true;
#elif BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
  // Extensions running in kiosk mode that have declared the "videoCapture"
  // permission in their manifest are allowed to control camera movements.
  if (IsPermissionGrantedForExtension(requesting_origin)) {
    *content_setting_result = CONTENT_SETTING_ALLOW;
    return true;
  }
  return false;
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
bool ChromeCameraPanTiltZoomPermissionContextDelegate::
    IsPermissionGrantedForExtension(const GURL& origin) const {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->enabled_extensions()
          .GetExtensionOrAppByURL(origin);

  extensions::KioskDelegate* const kiosk_delegate =
      extensions::ExtensionsBrowserClient::Get()->GetKioskDelegate();
  DCHECK(kiosk_delegate);

  if (!extension ||
      !extension->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kVideoCapture) ||
      !kiosk_delegate->IsAutoLaunchedKioskApp(extension->id())) {
    // The `extension` doesn't exist, doesn't have the "videoCapture"
    // permission declared in their manifest, or is not running in kiosk mode.
    return false;
  }

  return true;
}
#endif
