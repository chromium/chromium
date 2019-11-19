// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_EXTERNAL_PROTOCOL_DIALOG_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace syncer {
class DeviceInfo;
}  // namespace syncer

namespace arc {

using GurlAndActivityInfo =
    std::pair<GURL, ArcIntentHelperBridge::ActivityName>;

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

// These enums are used to define the buckets for an enumerated UMA histogram
// and need to be synced with enums.xml. This enum class should also be
// treated as append-only.
enum class ProtocolAction {
  OTHER_ACCEPTED_PERSISTED = 0,
  OTHER_ACCEPTED_NOT_PERSISTED = 1,
  OTHER_REJECTED = 2,
  BITCOIN_ACCEPTED_PERSISTED = 3,
  BITCOIN_ACCEPTED_NOT_PERSISTED = 4,
  BITCOIN_REJECTED = 5,
  GEO_ACCEPTED_PERSISTED = 6,
  GEO_ACCEPTED_NOT_PERSISTED = 7,
  GEO_REJECTED = 8,
  IM_ACCEPTED_PERSISTED = 9,
  IM_ACCEPTED_NOT_PERSISTED = 10,
  IM_REJECTED = 11,
  IRC_ACCEPTED_PERSISTED = 12,
  IRC_ACCEPTED_NOT_PERSISTED = 13,
  IRC_REJECTED = 14,
  MAGNET_ACCEPTED_PERSISTED = 15,
  MAGNET_ACCEPTED_NOT_PERSISTED = 16,
  MAGNET_REJECTED = 17,
  MAILTO_ACCEPTED_PERSISTED = 18,
  MAILTO_ACCEPTED_NOT_PERSISTED = 19,
  MAILTO_REJECTED = 20,
  MMS_ACCEPTED_PERSISTED = 21,
  MMS_ACCEPTED_NOT_PERSISTED = 22,
  MMS_REJECTED = 23,
  SIP_ACCEPTED_PERSISTED = 24,
  SIP_ACCEPTED_NOT_PERSISTED = 25,
  SIP_REJECTED = 26,
  SKYPE_ACCEPTED_PERSISTED = 27,
  SKYPE_ACCEPTED_NOT_PERSISTED = 28,
  SKYPE_REJECTED = 29,
  SMS_ACCEPTED_PERSISTED = 30,
  SMS_ACCEPTED_NOT_PERSISTED = 31,
  SMS_REJECTED = 32,
  SPOTIFY_ACCEPTED_PERSISTED = 33,
  SPOTIFY_ACCEPTED_NOT_PERSISTED = 34,
  SPOTIFY_REJECTED = 35,
  SSH_ACCEPTED_PERSISTED = 36,
  SSH_ACCEPTED_NOT_PERSISTED = 37,
  SSH_REJECTED = 38,
  TEL_ACCEPTED_PERSISTED = 39,
  TEL_ACCEPTED_NOT_PERSISTED = 40,
  TEL_REJECTED = 41,
  TELNET_ACCEPTED_PERSISTED = 42,
  TELNET_ACCEPTED_NOT_PERSISTED = 43,
  TELNET_REJECTED = 44,
  WEBCAL_ACCEPTED_PERSISTED = 45,
  WEBCAL_ACCEPTED_NOT_PERSISTED = 46,
  WEBCAL_REJECTED = 47,
  TEL_DEVICE_SELECTED = 48,
  kMaxValue = TEL_DEVICE_SELECTED
};

// Possible schemes for recording external protocol dialog metrics
enum class Scheme {
  BITCOIN,
  GEO,
  IM,
  IRC,
  MAGNET,
  MAILTO,
  MMS,
  SIP,
  SKYPE,
  SMS,
  SPOTIFY,
  SSH,
  TEL,
  TELNET,
  WEBCAL,
  OTHER
};

// Shows ARC version of the dialog. Returns true if ARC is supported, running,
// and in a context where it is allowed to handle external protocol.
bool RunArcExternalProtocolDialog(
    const GURL& url,
    const base::Optional<url::Origin>& initiating_origin,
    int render_process_host_id,
    int routing_id,
    ui::PageTransition page_transition,
    bool has_user_gesture);

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    GurlAndActivityInfo* out_url_and_activity_name,
    bool* safe_to_bypass_ui);

GURL GetUrlToNavigateOnDeactivateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers);

bool GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlagForTesting(
    content::WebContents* web_contents);

bool IsChromeAnAppCandidateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers);

void RecordUmaDialogAction(Scheme scheme,
                           apps::PickerEntryType entry_type,
                           bool accepted,
                           bool persisted);

ProtocolAction GetProtocolAction(Scheme scheme,
                                 apps::PickerEntryType entry_type,
                                 bool accepted,
                                 bool persisted);

void OnIntentPickerClosedForTesting(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    bool safe_to_bypass_ui,
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices,
    const std::string& selected_app_package,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason reason,
    bool should_persist);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_EXTERNAL_PROTOCOL_DIALOG_H_
