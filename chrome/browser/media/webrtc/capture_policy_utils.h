// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CAPTURE_POLICY_UTILS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CAPTURE_POLICY_UTILS_H_

#include <vector>

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "content/public/common/buildflags.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

class GURL;
class PrefRegistrySimple;
class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

// This enum represents the various levels in priority order from most
// restrictive to least restrictive, to which capture may be restricted by
// enterprise policy. It should not be used in Logs, so that it's order may be
// changed as needed.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.media
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class AllowedScreenCaptureLevel {
  kDisallowed = 0,
  kSameOrigin = 1,
  kTab = 2,
  kWindow = 3,
  kDesktop = 4,
  // kUnrestricted should be set to the value of the most permissive element
  // (kDesktop), but using the actual symbol breaks Java enum generation.
  kUnrestricted = 4,
};

namespace capture_policy {

#if BUILDFLAG(IS_CHROMEOS)
// This pref connects to the MultiScreenCaptureAllowedForUrls policy and will
// replace the deprecated GetDisplayMediaSetSelectAllScreensAllowedForUrls
// policy once the pivot to IWAs is complete.
inline static constexpr char kManagedMultiScreenCaptureAllowedForUrls[] =
    "profile.managed_multi_screen_capture_allowed_for_urls";
#endif

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

// Returns whether |capture_level| allows |request_origin| to capture the tab
// |web_contents|. This is the case if tabs may be captured at all and, if
// |capture_level| is kSameOrigin, the document in the tab's primary main frame
// is same-origin with |request_origin|.
//
// Use this to validate a tab that has been selected for capture. Unlike
// GetIncludableWebContentsFilter(), it only reflects capture policy, and not
// which tabs should be offered to the user.
bool IsTabCaptureAllowed(const url::Origin& request_origin,
                         AllowedScreenCaptureLevel capture_level,
                         content::WebContents* web_contents);

// Gets the appropriate DesktopMediaList::WebContentsFilter that should be run
// against every WebContents shown for pickers that include tabs. It excludes
// the tabs that IsTabCaptureAllowed() rejects for |request_origin| and
// |capture_level|, as well as tabs that are never offered as a capture source,
// such as document picture-in-picture windows.
DesktopMediaList::WebContentsFilter GetIncludableWebContentsFilter(
    const GURL& request_origin,
    AllowedScreenCaptureLevel capture_level);

// Modifies the passed in |media_types| by removing any that are not allowed at
// the specified |capture_level|. Relative Ordering of the remaining items is
// unchanged.
void FilterMediaList(std::vector<DesktopMediaList::Type>& media_types,
                     AllowedScreenCaptureLevel capture_level);

// Returns whether the given |media_type| is allowed to be captured at the
// specified |capture_level|.
bool IsMediaTypeAllowed(AllowedScreenCaptureLevel capture_level,
                        content::DesktopMediaID::Type media_type);

// Validates that |media_id| is allowed under |capture_level| and
// |request_origin|, and if it identifies a tab, that the tab still exists.
// Returns MediaStreamRequestResult::OK if allowed, or the corresponding error
// result.
blink::mojom::MediaStreamRequestResult ValidateSelectedMediaId(
    const content::DesktopMediaID& media_id,
    const url::Origin& request_origin,
    AllowedScreenCaptureLevel capture_level);

void ShowCaptureTerminatedDialog(content::WebContents* contents);

void RegisterProfilePrefs(PrefRegistrySimple* registry);

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
bool IsTransientActivationRequiredForGetDisplayMedia(
    content::WebContents* contents);
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

// Returns whether a capture that |request_origin| requests from |capturer| is
// restricted by policy to tabs that are same-origin with |request_origin|.
// |request_origin| is the origin of the frame that requests the capture, which
// is not necessarily the origin of |capturer|'s main frame.
bool CapturerRestrictedToSameOrigin(const url::Origin& request_origin,
                                    content::WebContents* capturer);

}  // namespace capture_policy

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CAPTURE_POLICY_UTILS_H_
