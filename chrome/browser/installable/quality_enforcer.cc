// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/strings/stringprintf.h"
#include "chrome/android/chrome_jni_headers/QualityEnforcer_jni.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace {

// Do not modify or reuse existing entries; they are used in a UMA histogram.
// Please also edit TrustedWebActivityQualityEnforcementViolationType in the
// enums.xml if adding new value.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browserservices
enum class QualityEnforcementViolationType {
  kHttpError404 = 0,
  kHttpError5xx = 1,
  kUnavailableOffline = 2,
  kDigitalAssetLink = 3,
  kMaxValue = kDigitalAssetLink
};

constexpr char kHttpErrorConsoleMessageFormat[] =
    "HTTP error %d when navigating to %s. Please make sure your "
    "app doesn’t have 404 or 5xx errors.";

constexpr char kUnavailableOfflineConsoleMessageFormat[] =
    "Page %s is not available offline. Please handle offline resource requests "
    "using a ServiceWorker. See: "
    "https://developers.google.com/web/fundamentals/codelabs/offline";

constexpr char kDigitalAssetLinkConsoleMessageFormat[] =
    "Launch URL %s failed digital asset link verification. Please review the "
    "Trusted Web Activity quick start guide for how to correctly implement "
    "Digital Assetlinks: "
    "https://developers.google.com/web/android/trusted-web-activity/"
    "quick-start";

}  // namespace

static void JNI_QualityEnforcer_ReportDevtoolsIssue(
    JNIEnv* env,
    const JavaParamRef<jobject>& jrender_frame_host,
    int type,
    const JavaParamRef<jstring>& jurl,
    int http_error_code) {
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  if (!render_frame_host)  // The frame is being unloaded.
    return;

  std::string url = base::android::ConvertJavaStringToUTF8(env, jurl);

  // TODO(crbug.com/1147479): Report the message in devtools issue tab instead.
  switch (static_cast<QualityEnforcementViolationType>(type)) {
    case QualityEnforcementViolationType::kHttpError404:
    case QualityEnforcementViolationType::kHttpError5xx:
      render_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(kHttpErrorConsoleMessageFormat, http_error_code,
                             url.c_str()));
      return;
    case QualityEnforcementViolationType::kUnavailableOffline:
      render_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(kUnavailableOfflineConsoleMessageFormat,
                             url.c_str()));
      return;
    case QualityEnforcementViolationType::kDigitalAssetLink:
      render_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(kDigitalAssetLinkConsoleMessageFormat,
                             url.c_str()));
      return;
  }
}
