// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_uma_util.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "jni/PermissionUmaUtil_jni.h"
#endif

// UMA keys need to be statically initialized so plain function would not
// work. Use macros instead.
#define PERMISSION_ACTION_UMA(secure_origin, permission, permission_secure, \
                              permission_insecure, action)                  \
  UMA_HISTOGRAM_ENUMERATION(permission, action, PermissionAction::NUM);     \
  if (secure_origin) {                                                      \
    UMA_HISTOGRAM_ENUMERATION(permission_secure, action,                    \
                              PermissionAction::NUM);                       \
  } else {                                                                  \
    UMA_HISTOGRAM_ENUMERATION(permission_insecure, action,                  \
                              PermissionAction::NUM);                       \
  }

#define PERMISSION_BUBBLE_TYPE_UMA(metric_name, permission_bubble_type) \
  UMA_HISTOGRAM_ENUMERATION(metric_name, permission_bubble_type,        \
                            PermissionRequestType::NUM)

#define PERMISSION_BUBBLE_GESTURE_TYPE_UMA(gesture_metric_name,              \
                                           no_gesture_metric_name,           \
                                           gesture_type,                     \
                                           permission_bubble_type)           \
  if (gesture_type == PermissionRequestGestureType::GESTURE) {               \
    PERMISSION_BUBBLE_TYPE_UMA(gesture_metric_name, permission_bubble_type); \
  } else if (gesture_type == PermissionRequestGestureType::NO_GESTURE) {     \
    PERMISSION_BUBBLE_TYPE_UMA(no_gesture_metric_name,                       \
                               permission_bubble_type);                      \
  }

using content::PermissionType;

namespace {

const int kPriorCountCap = 10;

std::string GetPermissionRequestString(PermissionRequestType type) {
  switch (type) {
    case PermissionRequestType::MULTIPLE:
      return "AudioAndVideoCapture";
    case PermissionRequestType::QUOTA:
      return "Quota";
    case PermissionRequestType::DOWNLOAD:
      return "MultipleDownload";
    case PermissionRequestType::REGISTER_PROTOCOL_HANDLER:
      return "RegisterProtocolHandler";
    case PermissionRequestType::PERMISSION_GEOLOCATION:
      return "Geolocation";
    case PermissionRequestType::PERMISSION_MIDI_SYSEX:
      return "MidiSysEx";
    case PermissionRequestType::PERMISSION_NOTIFICATIONS:
      return "Notifications";
    case PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMedia";
    case PermissionRequestType::PERMISSION_FLASH:
      return "Flash";
    case PermissionRequestType::PERMISSION_MEDIASTREAM_MIC:
      return "AudioCapture";
    case PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case PermissionRequestType::PERMISSION_CLIPBOARD_READ:
      return "ClipboardRead";
    case PermissionRequestType::PERMISSION_SECURITY_KEY_ATTESTATION:
      return "SecurityKeyAttestation";
    case PermissionRequestType::PERMISSION_PAYMENT_HANDLER:
      return "PaymentHandler";
    default:
      NOTREACHED();
      return "";
  }
}

void RecordEngagementMetric(const std::vector<PermissionRequest*>& requests,
                            const content::WebContents* web_contents,
                            const std::string& action) {
  PermissionRequestType type = requests[0]->GetPermissionRequestType();
  if (requests.size() > 1)
    type = PermissionRequestType::MULTIPLE;

  DCHECK(action == "Accepted" || action == "Denied" || action == "Dismissed" ||
         action == "Ignored");
  std::string name = "Permissions.Engagement." + action + '.' +
                     GetPermissionRequestString(type);

  SiteEngagementService* site_engagement_service = SiteEngagementService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  double engagement_score =
      site_engagement_service->GetScore(requests[0]->GetOrigin());

  base::UmaHistogramPercentage(name, engagement_score);
}

}  // anonymous namespace

// PermissionUmaUtil ----------------------------------------------------------

const char PermissionUmaUtil::kPermissionsPromptShown[] =
    "Permissions.Prompt.Shown";
const char PermissionUmaUtil::kPermissionsPromptShownGesture[] =
    "Permissions.Prompt.Shown.Gesture";
const char PermissionUmaUtil::kPermissionsPromptShownNoGesture[] =
    "Permissions.Prompt.Shown.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptAccepted[] =
    "Permissions.Prompt.Accepted";
const char PermissionUmaUtil::kPermissionsPromptAcceptedGesture[] =
    "Permissions.Prompt.Accepted.Gesture";
const char PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture[] =
    "Permissions.Prompt.Accepted.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptDenied[] =
    "Permissions.Prompt.Denied";
const char PermissionUmaUtil::kPermissionsPromptDeniedGesture[] =
    "Permissions.Prompt.Denied.Gesture";
const char PermissionUmaUtil::kPermissionsPromptDeniedNoGesture[] =
    "Permissions.Prompt.Denied.NoGesture";

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionUmaUtil::PermissionRequested(ContentSettingsType content_type,
                                            const GURL& requesting_origin) {
  PermissionType permission;
  bool success = PermissionUtil::GetPermissionType(content_type, &permission);
  DCHECK(success);

  bool secure_origin = content::IsOriginSecure(requesting_origin);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.PermissionRequested", permission,
                            PermissionType::NUM);
  if (secure_origin) {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_SecureOrigin", permission,
        PermissionType::NUM);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_InsecureOrigin", permission,
        PermissionType::NUM);
  }
}

void PermissionUmaUtil::PermissionRevoked(ContentSettingsType permission,
                                          PermissionSourceUI source_ui,
                                          const GURL& revoked_origin,
                                          Profile* profile) {
  // TODO(tsergeant): Expand metrics definitions for revocation to include all
  // permissions.
  if (permission == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
      permission == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
      permission == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      permission == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    // An unknown gesture type is passed in since gesture type is only
    // applicable in prompt UIs where revocations are not possible.
    RecordPermissionAction(permission, PermissionAction::REVOKED, source_ui,
                           PermissionRequestGestureType::UNKNOWN,
                           revoked_origin, /*web_contents=*/nullptr, profile);
  }
}

void PermissionUmaUtil::RecordEmbargoPromptSuppression(
    PermissionEmbargoStatus embargo_status) {
  UMA_HISTOGRAM_ENUMERATION("Permissions.AutoBlocker.EmbargoPromptSuppression",
                            embargo_status, PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(
    PermissionStatusSource source) {
  // Explicitly switch to ensure that any new PermissionStatusSource values are
  // dealt with appropriately.
  switch (source) {
    case PermissionStatusSource::MULTIPLE_DISMISSALS:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_DISMISSALS);
      break;
    case PermissionStatusSource::MULTIPLE_IGNORES:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_IGNORES);
      break;
    case PermissionStatusSource::UNSPECIFIED:
    case PermissionStatusSource::KILL_SWITCH:
    case PermissionStatusSource::INSECURE_ORIGIN:
    case PermissionStatusSource::FEATURE_POLICY:
    case PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
      // The permission wasn't under embargo, so don't record anything. We may
      // embargo it later.
      break;
  }
}

void PermissionUmaUtil::RecordEmbargoStatus(
    PermissionEmbargoStatus embargo_status) {
  UMA_HISTOGRAM_ENUMERATION("Permissions.AutoBlocker.EmbargoStatus",
                            embargo_status, PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::PermissionPromptShown(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK(!requests.empty());

  PermissionRequestType request_type = PermissionRequestType::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = requests[0]->GetPermissionRequestType();
    gesture_type = requests[0]->GetGestureType();
  }

  PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptShown, request_type);
  PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptShownGesture,
                                     kPermissionsPromptShownNoGesture,
                                     gesture_type, request_type);
}

void PermissionUmaUtil::PermissionPromptResolved(
    const std::vector<PermissionRequest*>& requests,
    const content::WebContents* web_contents,
    PermissionAction permission_action) {
  std::string action_string;

  switch (permission_action) {
    case PermissionAction::GRANTED:
      RecordPromptDecided(requests, /*accepted=*/true);
      action_string = "Accepted";
      break;
    case PermissionAction::DENIED:
      RecordPromptDecided(requests, /*accepted=*/false);
      action_string = "Denied";
      break;
    case PermissionAction::DISMISSED:
      action_string = "Dismissed";
      break;
    case PermissionAction::IGNORED:
      action_string = "Ignored";
      break;
    default:
      NOTREACHED();
      break;
  }
  RecordEngagementMetric(requests, web_contents, action_string);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionDecisionAutoBlocker::GetForProfile(profile);

  for (PermissionRequest* request : requests) {
    ContentSettingsType permission = request->GetContentSettingsType();
    // TODO(timloh): We only record these metrics for permissions which use
    // PermissionRequestImpl as the other subclasses don't support
    // GetGestureType and GetContentSettingsType.
    if (permission == CONTENT_SETTINGS_TYPE_DEFAULT)
      continue;

    PermissionRequestGestureType gesture_type = request->GetGestureType();
    const GURL& requesting_origin = request->GetOrigin();

    RecordPermissionAction(permission, permission_action,
                           PermissionSourceUI::PROMPT, gesture_type,
                           requesting_origin, web_contents, profile);

    std::string priorDismissPrefix =
        "Permissions.Prompt." + action_string + ".PriorDismissCount.";
    std::string priorIgnorePrefix =
        "Permissions.Prompt." + action_string + ".PriorIgnoreCount.";
    RecordPermissionPromptPriorCount(
        permission, priorDismissPrefix,
        autoblocker->GetDismissCount(requesting_origin, permission));
    RecordPermissionPromptPriorCount(
        permission, priorIgnorePrefix,
        autoblocker->GetIgnoreCount(requesting_origin, permission));
#if defined(OS_ANDROID)
    if (permission == CONTENT_SETTINGS_TYPE_GEOLOCATION &&
        permission_action != PermissionAction::IGNORED) {
      RecordWithBatteryBucket("Permissions.BatteryLevel." + action_string +
                              ".Geolocation");
    }
#endif
  }
}

void PermissionUmaUtil::RecordPermissionPromptPriorCount(
    ContentSettingsType permission,
    const std::string& prefix,
    int count) {
  // The user is not prompted for this permissions, thus there is no prompt
  // event to record a prior count for.
  DCHECK_NE(CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, permission);

  // Expand UMA_HISTOGRAM_COUNTS_100 so that we can use a dynamically suffixed
  // histogram name.
  base::Histogram::FactoryGet(
      prefix + PermissionUtil::GetPermissionString(permission), 1, 100, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(count);
}

#if defined(OS_ANDROID)
void PermissionUmaUtil::RecordWithBatteryBucket(const std::string& histogram) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionUmaUtil_recordWithBatteryBucket(
      env, base::android::ConvertUTF8ToJavaString(env, histogram));
}
#endif

void PermissionUmaUtil::RecordPermissionAction(
    ContentSettingsType permission,
    PermissionAction action,
    PermissionSourceUI source_ui,
    PermissionRequestGestureType gesture_type,
    const GURL& requesting_origin,
    const content::WebContents* web_contents,
    Profile* profile) {
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionDecisionAutoBlocker::GetForProfile(profile);
  int dismiss_count =
      autoblocker->GetDismissCount(requesting_origin, permission);
  int ignore_count = autoblocker->GetIgnoreCount(requesting_origin, permission);

  if (web_contents) {
    ukm::SourceId source_id =
        ukm::GetSourceIdForWebContentsDocument(web_contents);
    ukm::builders::Permission(source_id)
        .SetAction(static_cast<int64_t>(action))
        .SetGesture(static_cast<int64_t>(gesture_type))
        .SetPermissionType(permission)
        .SetPriorDismissals(std::min(kPriorCountCap, dismiss_count))
        .SetPriorIgnores(std::min(kPriorCountCap, ignore_count))
        .SetSource(static_cast<int64_t>(source_ui))
        .Record(ukm::UkmRecorder::Get());
  }

  bool secure_origin = content::IsOriginSecure(requesting_origin);

  switch (permission) {
    // Geolocation, MidiSysEx, Push, Media and Clipboard permissions are
    // disabled on insecure origins, so there's no need to record separate
    // metrics for secure/insecure.
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      UMA_HISTOGRAM_ENUMERATION("Permissions.Action.Geolocation", action,
                                PermissionAction::NUM);
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.Notifications",
                            "Permissions.Action.SecureOrigin.Notifications",
                            "Permissions.Action.InsecureOrigin.Notifications",
                            action);
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      UMA_HISTOGRAM_ENUMERATION("Permissions.Action.MidiSysEx", action,
                                PermissionAction::NUM);
      break;
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.ProtectedMedia",
                            "Permissions.Action.SecureOrigin.ProtectedMedia",
                            "Permissions.Action.InsecureOrigin.ProtectedMedia",
                            action);
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      UMA_HISTOGRAM_ENUMERATION("Permissions.Action.AudioCapture", action,
                                PermissionAction::NUM);
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      UMA_HISTOGRAM_ENUMERATION("Permissions.Action.VideoCapture", action,
                                PermissionAction::NUM);
      break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.Flash",
                            "Permissions.Action.SecureOrigin.Flash",
                            "Permissions.Action.InsecureOrigin.Flash", action);
      break;
    case CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
      UMA_HISTOGRAM_ENUMERATION("Permissions.Action.ClipboardRead", action,
                                PermissionAction::NUM);
      break;
    case CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER:
      UMA_HISTOGRAM_ENUMERATION("Permissions.Action.PaymentHandler", action,
                                PermissionAction::NUM);
      break;
    // The user is not prompted for these permissions, thus there is no
    // permission action recorded for them.
    default:
      NOTREACHED() << "PERMISSION "
                   << PermissionUtil::GetPermissionString(permission)
                   << " not accounted for";
  }
}

// static
void PermissionUmaUtil::RecordPromptDecided(
    const std::vector<PermissionRequest*>& requests,
    bool accepted) {
  DCHECK(!requests.empty());

  PermissionRequestType request_type = PermissionRequestType::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = requests[0]->GetPermissionRequestType();
    gesture_type = requests[0]->GetGestureType();
  }

  if (accepted) {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptAccepted, request_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptAcceptedGesture,
                                       kPermissionsPromptAcceptedNoGesture,
                                       gesture_type, request_type);
  } else {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptDenied, request_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptDeniedGesture,
                                       kPermissionsPromptDeniedNoGesture,
                                       gesture_type, request_type);
  }
}
