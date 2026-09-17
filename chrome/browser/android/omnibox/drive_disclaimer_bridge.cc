// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/android/android_info.h"
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/contextual_search/consent_kit/consent_kit_result_parser.h"
#include "components/contextual_search/consent_kit/consent_kit_url_builder.h"
#include "components/contextual_search/consent_kit/proto/iframe_interface.pb.h"
#include "components/contextual_search/footprints/public/drive_disclaimer_controller.h"
#include "components/contextual_search/footprints/public/fpop_service.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/gurl.h"
#include "url/origin.h"

using DisclaimerController = drive_picker::DriveDisclaimerController;
using DisclaimerStatus = DisclaimerController::DisclaimerStatus;

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<DisclaimerStatus>(
    JNIEnv* env,
    const DisclaimerStatus& val) {
  return ToJavaInteger(env, std::to_underlying(val));
}

}  // namespace jni_zero

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/DriveDisclaimerBridge_jni.h"

namespace {

constexpr char kConsentKitOrigin[] = "https://consent.google.com";

using StatusCallback = base::OnceCallback<void(DisclaimerStatus)>;

contextual_search::DriveConsentState ToConsentState(DisclaimerStatus status) {
  switch (status) {
    case DisclaimerStatus::kAccepted:
      return contextual_search::DriveConsentState::kConsent;
    case DisclaimerStatus::kNotAccepted:
      return contextual_search::DriveConsentState::kNotConsent;
    case DisclaimerStatus::kRestricted:
      return contextual_search::DriveConsentState::kRestricted;
  }
}

void ReportStatus(Profile* profile,
                  StatusCallback callback,
                  std::unique_ptr<DisclaimerController> controller,
                  DisclaimerStatus status) {
  if (profile) {
    profile->GetPrefs()->SetInteger(contextual_search::kDriveConsentState,
                                    std::to_underlying(ToConsentState(status)));
  }
  std::move(callback).Run(status);
}

// Appends the ConsentKit WebView identifier token to the User-Agent string so
// that the server activates the Android WebView JS bridge.
// Matches getUserAgent() in:
// google3/java/com/google/android/libraries/onegoogle/consent/presentation/web/
// CustomWebViewConsentDialogFragment.kt
std::string BuildConsentKitUserAgent(const std::string& base_user_agent,
                                     int sdk_int,
                                     bool is_dark_mode) {
  base::DictValue versions;
  versions.Set("os", "Android");
  versions.Set("osVersion", base::NumberToString(sdk_int));
  versions.Set("isDarkTheme", is_dark_mode);
  std::string versions_json;
  base::JSONWriter::Write(versions, &versions_json);
  base::ReplaceChars(versions_json, "()", "_", &versions_json);
  return base_user_agent + " CkIdWebView (" + versions_json + ")";
}

}  // namespace

// Asynchronously resolves the user's Drive consent status against the
// Footprints (FACS) backend. The backend is the source of truth, since consent
// may have been granted or revoked on another device.
static void JNI_DriveDisclaimerBridge_CheckConsentStatus(
    JNIEnv* env,
    Profile* profile,
    StatusCallback callback) {
  if (!profile) {
    std::move(callback).Run(DisclaimerStatus::kRestricted);
    return;
  }

  // Test override that bypasses the backend round trip entirely.
  if (base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted)) {
    ReportStatus(profile, std::move(callback), /*controller=*/nullptr,
                 DisclaimerStatus::kAccepted);
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    ReportStatus(profile, std::move(callback), /*controller=*/nullptr,
                 DisclaimerStatus::kRestricted);
    return;
  }

  auto controller = std::make_unique<DisclaimerController>(
      contextual_search::FpopService::Create(
          identity_manager, profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess()));
  auto* controller_ptr = controller.get();
  controller_ptr->CheckDisclaimerStatusAsync(base::BindOnce(
      &ReportStatus, profile, std::move(callback), std::move(controller)));
}

static std::string JNI_DriveDisclaimerBridge_GetConsentUrl(JNIEnv* env,
                                                           Profile* profile,
                                                           bool is_dark_mode) {
  if (!profile) {
    return std::string();
  }

  drive::ConsentKitUrlBuilder builder;
  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(profile)) {
    builder.SetSessionIndex(static_cast<int>(
        identity_manager->GetSessionIndexForPrimaryAccount().value_or(0u)));
  }
  builder.SetLocale(
      g_browser_process->GetFeatures()->application_locale_storage()->Get());
  builder.SetFlowId(omnibox::kComposeboxDriveConsentFlowId.Get());
  builder.SetProductId(omnibox::kComposeboxDriveConsentProductId.Get());
  builder.SetProductSurface(
      omnibox::kComposeboxDriveConsentProductSurface.Get());
  builder.SetEntrypointId(omnibox::kComposeboxDriveConsentEntrypointId.Get());
  builder.SetDarkMode(is_dark_mode);
  builder.SetUseWebViewEndpoint(true);
  return builder.Build().spec();
}

static void JNI_DriveDisclaimerBridge_SetConsentKitUserAgent(
    JNIEnv* env,
    content::WebContents* web_contents,
    bool is_dark_mode) {
  if (!web_contents) {
    return;
  }
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = BuildConsentKitUserAgent(
      embedder_support::GetUserAgent(), base::android::android_info::sdk_int(),
      is_dark_mode);
  ua_override.ua_metadata_override = embedder_support::GetUserAgentMetadata();
  web_contents->SetUserAgentOverride(ua_override,
                                     /*override_in_new_tabs=*/false);
}

static bool JNI_DriveDisclaimerBridge_ParseAndSaveConsentResult(
    JNIEnv* env,
    Profile* profile,
    content::WebContents* web_contents,
    const std::string& base64_result) {
  if (!profile || !web_contents) {
    return false;
  }

  // Result must originate from ConsentKit.
  if (!web_contents->GetPrimaryMainFrame()
           ->GetLastCommittedOrigin()
           .IsSameOriginWith(GURL(kConsentKitOrigin))) {
    return false;
  }

  std::string binary_proto;
  if (!base::Base64UrlDecode(base64_result,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &binary_proto)) {
    return false;
  }

  identity_consent::PrivacyFlowResult flow_result;
  if (!flow_result.ParseFromString(binary_proto) ||
      !drive::IsExpectedConsentFlow(
          flow_result, omnibox::kComposeboxDriveConsentFlowId.Get()) ||
      !drive::HasGrantedDriveConsent(flow_result)) {
    return false;
  }

  profile->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      std::to_underlying(contextual_search::DriveConsentState::kConsent));
  return true;
}

DEFINE_JNI(DriveDisclaimerBridge)
