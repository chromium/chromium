// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_EXTERNAL_PROTOCOL_DIALOG_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"
#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

class GURL;
class SharingTargetDeviceInfo;

namespace content {
class WebContents;
}  // namespace content

namespace arc {

using GurlAndActivityInfo = std::pair<GURL, ArcIconCacheDelegate::ActivityName>;

// An enum returned from GetAction function. This is visible for testing.
enum class GetActionResult {
  // ARC cannot handle the |original_url|, but the URL did have a fallback URL
  // which Chrome can handle. Chrome should show the fallback URL now.
  OPEN_URL_IN_CHROME,
  // ARC can handle the |original_url|, and one of the ARC activities is a
  // preferred one. ARC should handle the URL now.
  HANDLE_URL_IN_ARC,
  // Chrome should show the disambig UI because 1) ARC can handle the
  // |original_url| but none of the ARC activities is a preferred one, or
  // 2) there are two or more browsers (e.g. Chrome and a browser app in ARC)
  // that can handle a fallback URL.
  ASK_USER,
};

// Checks if ARC is supported, running, and in a context where it is allowed to
// handle external protocol, then either shows the dialog, or directly launches
// the app if a user has previously made a choice. Invokes |handled_cb| with
// true if the protocol has been handled by ARC.
void RunArcExternalProtocolDialog(
    const GURL& url,
    const std::optional<url::Origin>& initiating_origin,
    base::WeakPtr<content::WebContents> web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    base::OnceCallback<void(bool)> handled_cb);

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>& handlers,
    size_t selected_app_index,
    GurlAndActivityInfo* out_url_and_activity_name,
    bool* safe_to_bypass_ui);

GURL GetUrlToNavigateOnDeactivateForTesting(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers);

bool GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlagForTesting(
    content::WebContents* web_contents);

bool IsChromeAnAppCandidateForTesting(
    const std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>&
        handlers);

void OnIntentPickerClosedForTesting(
    base::WeakPtr<content::WebContents> web_contents,
    const GURL& url,
    bool safe_to_bypass_ui,
    std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate,
    std::vector<SharingTargetDeviceInfo> devices,
    const std::string& selected_app_package,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason reason,
    bool should_persist);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_EXTERNAL_PROTOCOL_DIALOG_H_
