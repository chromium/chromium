// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/annotation_control_provider.h"

#include "base/containers/flat_map.h"
#include "components/policy/policy_constants.h"

namespace {

constexpr char kAutofillQueryAnnotationHash[] = "88863520";
constexpr char kAutofillUploadAnnotationHash[] = "104798869";
constexpr char kCalendarGetEventsAnnotationHash[] = "86429515";
constexpr char kChromeFeedbackReportAppAnnotationHash[] = "134729048";
constexpr char kChromePluginVmApiAnnotationHash[] = "28498700";
constexpr char kDomainReliabilityReportUploadAnnotationHash[] = "108804096";
constexpr char kQuickAnswersLoaderAnnotationHash[] = "46208118";
constexpr char kRemotingLogToServerAnnotationHash[] = "99742369";
constexpr char kSafeBrowsingBinaryUploadAppAnnotationHash[] = "4306022";
constexpr char kSignedInProfileAvatarAnnotationHash[] = "108903331";
constexpr char kWallpaperDownloadGooglePhotoAnnotationHash[] = "50127013";
constexpr char kWebrtcEventLogUploaderAnnotationHash[] = "24186190";
constexpr char kWebrtcLogUploadAnnotationHash[] = "62443804";

}  // namespace

namespace policy {

AnnotationControlProvider::AnnotationControlProvider() = default;
AnnotationControlProvider::~AnnotationControlProvider() = default;

base::flat_map<std::string, AnnotationControl>
AnnotationControlProvider::GetControls() {
  // Lazy init.
  if (annotation_controls_.empty()) {
    Load();
  }

  return annotation_controls_;
}

// Setup annotation to policy mappings with some hand-picked network
// annotations. Annotations are keyed by hash codes which are generated at
// compile time. There is also a helper script to generate these hash codes at:
// `tools/traffic_annotation/scripts/auditor/README.md`
void AnnotationControlProvider::Load() {
  // autofill_query
  // Note: This one is purposefully incorrect to allow for initial testing. It
  //       should have the same policies as 'autofill_upload' below.
  annotation_controls_[kAutofillQueryAnnotationHash] =
      AnnotationControl().Add(key::kPasswordManagerEnabled, base::Value(false));

  // autofill_upload
  annotation_controls_[kAutofillUploadAnnotationHash] =
      AnnotationControl()
          .Add(key::kPasswordManagerEnabled, base::Value(false))
          .Add(key::kAutofillAddressEnabled, base::Value(false))
          .Add(key::kAutofillCreditCardEnabled, base::Value(false));

  // calendar_get_events
  annotation_controls_[kCalendarGetEventsAnnotationHash] =
      AnnotationControl().Add(key::kCalendarIntegrationEnabled,
                              base::Value(false));

  // chrome_feedback_report_app
  annotation_controls_[kChromeFeedbackReportAppAnnotationHash] =
      AnnotationControl().Add(key::kUserFeedbackAllowed, base::Value(false));

  // chrome_plugin_vm_api
  annotation_controls_[kChromePluginVmApiAnnotationHash] =
      AnnotationControl().Add(key::kUserPluginVmAllowed, base::Value(false));

  // domain_reliability_report_upload
  annotation_controls_[kDomainReliabilityReportUploadAnnotationHash] =
      AnnotationControl().Add(key::kDomainReliabilityAllowed,
                              base::Value(false));

  // quick_answers_loader
  annotation_controls_[kQuickAnswersLoaderAnnotationHash] =
      AnnotationControl()
          .Add(key::kQuickAnswersDefinitionEnabled, base::Value(false))
          .Add(key::kQuickAnswersUnitConversionEnabled, base::Value(false));

  // remoting_log_to_server
  annotation_controls_[kRemotingLogToServerAnnotationHash] =
      AnnotationControl()
          .Add(key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
               base::Value(false))
          .Add(key::kRemoteAccessHostAllowRemoteSupportConnections,
               base::Value(false));

  // safe_browsing_binary_upload_app
  annotation_controls_[kSafeBrowsingBinaryUploadAppAnnotationHash] =
      AnnotationControl().Add(key::kAdvancedProtectionAllowed,
                              base::Value(false));

  // signed_in_profile_avatar
  annotation_controls_[kSignedInProfileAvatarAnnotationHash] =
      AnnotationControl().Add(key::kUserAvatarCustomizationSelectorsEnabled,
                              base::Value(false));

  // wallpaper_download_google_photo
  annotation_controls_[kWallpaperDownloadGooglePhotoAnnotationHash] =
      AnnotationControl().Add(key::kWallpaperGooglePhotosIntegrationEnabled,
                              base::Value(false));

  // webrtc_event_log_uploader
  annotation_controls_[kWebrtcEventLogUploaderAnnotationHash] =
      AnnotationControl().Add(key::kWebRtcEventLogCollectionAllowed,
                              base::Value(false));

  // webrtc_log_upload
  annotation_controls_[kWebrtcLogUploadAnnotationHash] =
      AnnotationControl().Add(key::kWebRtcTextLogCollectionAllowed,
                              base::Value(false));
}

}  // namespace policy
