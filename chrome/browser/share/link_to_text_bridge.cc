// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/LinkToTextBridge_jni.h"

using shared_highlighting::LinkGenerationReadyStatus;
using shared_highlighting::LinkGenerationStatus;

namespace {

ukm::SourceId GetSourceId(
    const base::android::JavaRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  return web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
}
}  // namespace

// TODO(gayane): Update the name whenever
// |ShouldOfferLinkToText| updated to more descriptive
// name.
static bool JNI_LinkToTextBridge_ShouldOfferLinkToText(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return shared_highlighting::ShouldOfferLinkToText(url);
}

static bool JNI_LinkToTextBridge_SupportsLinkGenerationInIframe(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return shared_highlighting::SupportsLinkGenerationInIframe(url);
}

static void JNI_LinkToTextBridge_LogFailureMetrics(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_web_contents,
    int32_t error) {
  shared_highlighting::LogRequestedFailureMetrics(
      GetSourceId(j_web_contents),
      static_cast<shared_highlighting::LinkGenerationError>(error));
}

static void JNI_LinkToTextBridge_LogSuccessMetrics(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_web_contents) {
  shared_highlighting::LogRequestedSuccessMetrics(GetSourceId(j_web_contents));
}

static void JNI_LinkToTextBridge_LogLinkRequestedBeforeStatus(
    JNIEnv* env,
    int32_t status,
    int32_t ready_status) {
  shared_highlighting::LogLinkRequestedBeforeStatus(
      static_cast<LinkGenerationStatus>(status),
      static_cast<LinkGenerationReadyStatus>(ready_status));
}

static void JNI_LinkToTextBridge_LogLinkToTextReshareStatus(JNIEnv* env,
                                                            int32_t status) {
  shared_highlighting::LogLinkToTextReshareStatus(
      static_cast<shared_highlighting::LinkToTextReshareStatus>(status));
}

DEFINE_JNI(LinkToTextBridge)
