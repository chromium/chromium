// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_crash_keys.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_allowlist_manager.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "components/google/core/common/google_util.h"
#include "components/security_interstitials/core/urls.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_database.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentsStatics_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void ClientCertificatesCleared(const JavaRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_clientCertificatesCleared(env, callback);
}

void NotifyClientCertificatesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
}

void SafeBrowsingAllowlistAssigned(const JavaRef<jobject>& callback,
                                   bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_safeBrowsingAllowlistAssigned(env, callback, success);
}

}  // namespace

// static
ScopedJavaLocalRef<jstring>
JNI_AwContentsStatics_GetSafeBrowsingPrivacyPolicyUrl(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL privacy_policy_url(
      security_interstitials::kSafeBrowsingPrivacyPolicyUrl);
  std::string locale =
      AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager()->app_locale();
  privacy_policy_url =
      google_util::AppendGoogleLocaleParam(privacy_policy_url, locale);
  return base::android::ConvertUTF8ToJavaString(env, privacy_policy_url.spec());
}

// static
void JNI_AwContentsStatics_ClearClientCertPreferences(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&NotifyClientCertificatesChanged),
      base::BindOnce(&ClientCertificatesCleared,
                     ScopedJavaGlobalRef<jobject>(env, callback)));
}

// static
ScopedJavaLocalRef<jstring> JNI_AwContentsStatics_GetUnreachableWebDataUrl(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, content::kUnreachableWebDataURL);
}

// static
ScopedJavaLocalRef<jstring> JNI_AwContentsStatics_GetProductVersion(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, version_info::GetVersionNumber());
}

// static
void JNI_AwContentsStatics_SetSafeBrowsingAllowlist(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jrules,
    const JavaParamRef<jobject>& callback) {
  std::vector<std::string> rules;
  base::android::AppendJavaStringArrayToStringVector(env, jrules, &rules);
  AwSafeBrowsingAllowlistManager* allowlist_manager =
      AwBrowserProcess::GetInstance()->GetSafeBrowsingAllowlistManager();
  allowlist_manager->SetAllowlistOnUIThread(
      std::move(rules),
      base::BindOnce(&SafeBrowsingAllowlistAssigned,
                     ScopedJavaGlobalRef<jobject>(env, callback)));
}

// static
void JNI_AwContentsStatics_SetCheckClearTextPermitted(
    JNIEnv* env,
    jboolean permitted) {
  AwContentBrowserClient::set_check_cleartext_permitted(permitted);
}

// static
void JNI_AwContentsStatics_LogCommandLineForDebugging(JNIEnv* env) {
  // Note: this should only be called for debugging purposes, since this is
  // *very* spammy.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  for (const auto& pair : command_line.GetSwitches()) {
    const std::string& key = pair.first;
    const base::CommandLine::StringType& value = pair.second;
    LOG(INFO) << "WebViewCommandLine '" << key << "': '" << value << "'";
  }
}

// static
void JNI_AwContentsStatics_LogFlagMetrics(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jswitches,
    const JavaParamRef<jobjectArray>& jfeatures) {
  std::set<std::string> switches;
  for (const auto& jswitch : jswitches.ReadElements<jstring>()) {
    switches.insert(ConvertJavaStringToUTF8(jswitch));
  }
  std::set<std::string> features;
  for (const auto& jfeature : jfeatures.ReadElements<jstring>()) {
    features.insert(ConvertJavaStringToUTF8(jfeature));
  }

  flags_ui::ReportAboutFlagsHistogram("Launch.FlagsAtStartup", switches,
                                      features);
  SetCrashKeysFromFeaturesAndSwitches(switches, features);
}

// static
jboolean JNI_AwContentsStatics_IsMultiProcessEnabled(JNIEnv* env) {
  return !content::RenderProcessHost::run_renderer_in_process();
}

// static
ScopedJavaLocalRef<jstring> JNI_AwContentsStatics_GetVariationsHeader(
    JNIEnv* env) {
  const bool is_signed_in = false;
  auto headers =
      variations::VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          is_signed_in);
  if (!headers)
    return base::android::ConvertUTF8ToJavaString(env, "");
  return base::android::ConvertUTF8ToJavaString(
      env,
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY));
}

}  // namespace android_webview
