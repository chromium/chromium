// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/UkmRecorder_jni.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"

namespace metrics {

// Called by Java org.chromium.chrome.browser.metrics.UkmRecorder.
static void JNI_UkmRecorder_RecordEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_event_name) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  const std::string event_name(ConvertJavaStringToUTF8(env, j_event_name));
  ukm::UkmEntryBuilder builder(source_id, event_name);
  builder.SetMetric("HasOccurred", true);
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace metrics
