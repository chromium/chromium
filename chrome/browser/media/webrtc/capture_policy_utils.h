// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CAPTURE_POLICY_UTILS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CAPTURE_POLICY_UTILS_H_

#include <vector>

#include "chrome/browser/media/webrtc/desktop_media_list.h"

class GURL;
class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace crosapi::mojom {
class MultiCaptureService;
}  // namespace crosapi::mojom

// This enum represents the various levels in priority order from most
// restrictive to least restrictive, to which capture may be restricted by
// enterprise policy. It should not be used in Logs, so that it's order may be
// changed as needed.
enum class AllowedScreenCaptureLevel {
  kDisallowed = 0,
  kSameOrigin = 1,
  kTab = 2,
  kWindow = 3,
  kDesktop = 4,
  kUnrestricted = kDesktop,
};

namespace capture_policy {

extern const char kManagedAccessToGetAllScreensMediaAllowedForUrls[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kManagedMultiScreenCaptureAllowedForUrls[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// Sets a multi capture service mock for testing.
void SetMultiCaptureServiceForTesting(
    crosapi::mojom::MultiCaptureService* service);

crosapi::mojom::MultiCaptureService* GetMultiCaptureService();
#endif  // BUILDFLAG(IS_CHROMEOS)

// Gets the highest capture level that the requesting origin is allowed to
// request based on any configured enterprise policies. This is a convenience
// overload which extracts the PrefService from the WebContents.
AllowedScreenCaptureLevel GetAllowedCaptureLevel(
    const GURL& request_origin,
    content::WebContents* capturer_web_contents);

// Gets the highest capture level that the requesting origin is allowed to
// request based on any configured enterprise policies.
AllowedScreenCaptureLevel GetAllowedCaptureLevel(const GURL& request_origin,
                                                 const PrefService& prefs);

// Gets the appropriate DesktopMediaList::WebContentsFilter that should be run
// against every WebContents shown for pickers that include tabs. Functionally
// this returns a no-op unless |capture_level| is kSameOrigin or kDisallowed.
// In the case of the latter, it always returns false, and for the former it
// checks that the WebContents's origin matches |request_origin|.
DesktopMediaList::WebContentsFilter GetIncludableWebContentsFilter(
    const GURL& request_origin,
    AllowedScreenCaptureLevel capture_level);

// Modifies the passed in |media_types| by removing any that are not allowed at
// the specified |capture_level|. Relative Ordering of the remaining items is
// unchanged.
void FilterMediaList(std::vector<DesktopMediaList::Type>& media_types,
                     AllowedScreenCaptureLevel capture_level);

void ShowCaptureTerminatedDialog(content::WebContents* contents);

void RegisterProfilePrefs(PrefRegistrySimple* registry);

// TODO(crbug.com/40230867): Use Origin instead of GURL.
void CheckGetAllScreensMediaAllowed(content::BrowserContext* context,
                                    const GURL& url,
                                    base::OnceCallback<void(bool)> callback);

void CheckGetAllScreensMediaAllowedForAnyOrigin(
    content::BrowserContext* context,
    base::OnceCallback<void(bool)> callback);

#if !BUILDFLAG(IS_ANDROID)
bool IsTransientActivationRequiredForGetDisplayMedia(
    content::WebContents* contents);
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace capture_policy

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CAPTURE_POLICY_UTILS_H_
