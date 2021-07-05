// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/platform_verification_chromeos.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace platform_verification {
namespace {

// These enum values must not change as they're used for UMA logging.
enum class Result {
  kSuccess = 0,             // The browser checks succeeded.
  kInvalidURL = 1,          // The URL was invalid.
  kUnsupportedProfile = 2,  // The profile type does not support RA.
  kUserRejected = 3,        // The user explicitly rejected the operation.
  kMaxValue = kUserRejected
};

const char kAttestationBrowserResultHistogram[] =
    "ChromeOS.PlatformVerification.BrowserResult";

void ReportResult(Result result) {
  UMA_HISTOGRAM_ENUMERATION(kAttestationBrowserResultHistogram, result);
}

// Gets the URL associated with the web contents for the purpose of platform
// verification.
GURL GetURL(content::WebContents* web_contents) {
  // TODO(xhwang): Using GetLastCommittedURL() here is not right. The platform
  // verification may be requested by a frame from a different origin.
  // https://crbug.com/1223920.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!url.is_valid())
    return web_contents->GetVisibleURL();
  return url;
}

// Whether platform verification is permitted by the user-configurable content
// setting.
bool IsPermittedByContentSettings(content::WebContents* web_contents) {
  const GURL& requesting_origin = GetURL(web_contents).GetOrigin();

  GURL embedding_origin =
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
  ContentSetting content_setting =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))
          ->GetPermissionStatus(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                                requesting_origin, embedding_origin)
          .content_setting;

  return content_setting == CONTENT_SETTING_ALLOW;
}

// Whether platform verification is permitted by the profile type. Currently
// platform verification is disabled for incognito and guest profiles.
bool IsPermittedByProfileType(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return !profile->IsOffTheRecord() && !profile->IsGuestSession();
}

}  // namespace

bool PerformBrowserChecks(content::WebContents* web_contents) {
  GURL url = GetURL(web_contents);
  if (!url.is_valid()) {
    ReportResult(Result::kInvalidURL);
    return false;
  }

  if (!IsPermittedByProfileType(web_contents)) {
    ReportResult(Result::kUnsupportedProfile);
    return false;
  }

  if (!IsPermittedByContentSettings(web_contents)) {
    ReportResult(Result::kUserRejected);
    return false;
  }

  ReportResult(Result::kSuccess);
  return true;
}

}  // namespace platform_verification
