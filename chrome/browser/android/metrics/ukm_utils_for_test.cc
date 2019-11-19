// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/stl_util.h"
#include "chrome/browser/android/metrics/jni_headers/UkmUtilsForTest_jni.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/ukm/ukm_service.h"

#include "chrome/browser/android/metrics/ukm_utils_for_test.h"

using base::android::JavaParamRef;

namespace ukm {

// static
bool UkmUtilsForTest::IsEnabled() {
  auto* service =
      g_browser_process->GetMetricsServicesManager()->GetUkmService();
  return service ? service->recording_enabled_ : false;
}

// static
bool UkmUtilsForTest::HasSourceWithId(SourceId source_id) {
  auto* service =
      g_browser_process->GetMetricsServicesManager()->GetUkmService();
  DCHECK(service);
  return base::Contains(service->sources(), source_id);
}

// static
void UkmUtilsForTest::RecordSourceWithId(SourceId source_id) {
  auto* service =
      g_browser_process->GetMetricsServicesManager()->GetUkmService();
  DCHECK(service);
  service->UpdateSourceURL(source_id, GURL("http://example.com"));
}

// static
uint64_t UkmUtilsForTest::GetClientId() {
  auto* service =
      g_browser_process->GetMetricsServicesManager()->GetUkmService();
  DCHECK(service);
  return service->client_id_;
}

}  // namespace ukm

static jboolean JNI_UkmUtilsForTest_IsEnabled(JNIEnv*) {
  return ukm::UkmUtilsForTest::IsEnabled();
}

static jboolean JNI_UkmUtilsForTest_HasSourceWithId(JNIEnv*,
                                                    jlong source_id) {
  ukm::SourceId source = static_cast<ukm::SourceId>(source_id);
  return ukm::UkmUtilsForTest::HasSourceWithId(source);
}

static void JNI_UkmUtilsForTest_RecordSourceWithId(JNIEnv*,
                                                   jlong source_id) {
  ukm::SourceId source = static_cast<ukm::SourceId>(source_id);
  ukm::UkmUtilsForTest::RecordSourceWithId(source);
}

static jlong JNI_UkmUtilsForTest_GetClientId(JNIEnv*) {
  return ukm::UkmUtilsForTest::GetClientId();
}
