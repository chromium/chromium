// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "chrome/android/chrome_jni_headers/QualityEnforcer_jni.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace {

// Do not modify or reuse existing entries; they are used in a UMA histogram.
// Please also edit TrustedWebActivityQualityEnforcementViolationType in the
// enums.xml if adding new value.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.browserservices.constants)
enum class QualityEnforcementViolationType {
  kHttpError404 = 0,
  kHttpError5xx = 1,
  kUnavailableOffline = 2,
  kDigitalAssetLink = 3,
  kMaxValue = kDigitalAssetLink
};

blink::mojom::TwaQualityEnforcementViolationType GetViolationType(
    QualityEnforcementViolationType type) {
  switch (type) {
    case QualityEnforcementViolationType::kHttpError404:
    case QualityEnforcementViolationType::kHttpError5xx:
      return blink::mojom::TwaQualityEnforcementViolationType ::kHttpError;
    case QualityEnforcementViolationType::kUnavailableOffline:
      return blink::mojom::TwaQualityEnforcementViolationType ::
          kUnavailableOffline;
    case QualityEnforcementViolationType::kDigitalAssetLink:
      return blink::mojom::TwaQualityEnforcementViolationType ::
          kDigitalAssetLinks;
  }
}
}  // namespace

static void JNI_QualityEnforcer_ReportDevtoolsIssue(
    JNIEnv* env,
    const JavaParamRef<jobject>& jrender_frame_host,
    int type,
    const JavaParamRef<jstring>& jurl,
    int http_status_code,
    const JavaParamRef<jstring>& jpackage_name,
    const JavaParamRef<jstring>& jsignature) {
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  if (!render_frame_host)  // The frame is being unloaded.
    return;

  std::string url = base::android::ConvertJavaStringToUTF8(env, jurl);
  absl::optional<std::string> package_name =
      jpackage_name
          ? absl::make_optional(
                base::android::ConvertJavaStringToUTF8(env, jpackage_name))
          : absl::nullopt;
  absl::optional<std::string> signature =
      jsignature ? absl::make_optional(
                       base::android::ConvertJavaStringToUTF8(env, jsignature))
                 : absl::nullopt;

  auto details = blink::mojom::InspectorIssueDetails::New();
  auto twa_issue = blink::mojom::TrustedWebActivityIssueDetails::New(
      GURL(url),
      GetViolationType(static_cast<QualityEnforcementViolationType>(type)),
      http_status_code, std::move(package_name), std::move(signature));
  details->twa_issue_details = std::move(twa_issue);
  render_frame_host->ReportInspectorIssue(blink::mojom::InspectorIssueInfo::New(
      blink::mojom::InspectorIssueCode::kTrustedWebActivityIssue,
      std::move(details)));
}
