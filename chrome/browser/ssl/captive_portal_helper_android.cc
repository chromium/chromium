// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_helper_android.h"
#include "base/task/post_task.h"
#include "chrome/browser/ssl/captive_portal_helper.h"
#include "content/public/browser/browser_task_traits.h"

#include <stddef.h>

#include <memory>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/android/chrome_jni_headers/CaptivePortalHelper_jni.h"
#include "chrome/browser/ssl/ssl_error_assistant.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "content/public/browser/browser_thread.h"
#include "net/android/network_library.h"

namespace chrome {
namespace android {

void JNI_CaptivePortalHelper_SetCaptivePortalCertificateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jhash) {
  auto default_proto =
      SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle();
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(SSLErrorHandler::SetErrorAssistantProto,
                                std::move(default_proto)));

  const std::string hash = ConvertJavaStringToUTF8(env, jhash);
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(INT_MAX);
  config_proto->add_captive_portal_cert()->set_sha256_hash(hash);

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(SSLErrorHandler::SetErrorAssistantProto,
                                std::move(config_proto)));
}

void JNI_CaptivePortalHelper_SetOSReportsCaptivePortalForTesting(
    JNIEnv* env,
    jboolean os_reports_captive_portal) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(SSLErrorHandler::SetOSReportsCaptivePortalForTesting,
                     os_reports_captive_portal));
}

void ReportNetworkConnectivity(JNIEnv* env) {
  Java_CaptivePortalHelper_reportNetworkConnectivity(env);
}

std::string GetCaptivePortalServerUrl(JNIEnv* env) {
  return base::android::ConvertJavaStringToUTF8(
      Java_CaptivePortalHelper_getCaptivePortalServerUrl(env));
}

}  // namespace android

bool IsBehindCaptivePortal() {
  return net::android::GetIsCaptivePortal();
}

}  // namespace chrome
