// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_metrics_service_client.h"

#include <jni.h>
#include <stdint.h>
#include <vector>

#include "android_webview/browser/aw_metrics_log_uploader.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/jni/AwMetricsServiceClient_jni.h"
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/hash.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

base::LazyInstance<AwMetricsServiceClient>::Leaky g_lazy_instance_;

namespace {

const int kUploadIntervalMinutes = 30;

// A GUID in text form is composed of 32 hex digits and 4 hyphens. These values
// must match those in AwMetricsServiceClient.java.
const size_t kGuidSize = 32 + 4;
const char* const kGuidFileName = "metrics_guid";

// Client ID of the app, read and cached synchronously at startup
base::LazyInstance<std::string>::Leaky g_client_id = LAZY_INSTANCE_INITIALIZER;

// Callbacks for metrics::MetricsStateManager::Create. Store/LoadClientInfo
// allow Windows Chrome to back up ClientInfo. They're no-ops for WebView.

void StoreClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadClientInfo() {
  std::unique_ptr<metrics::ClientInfo> client_info;
  return client_info;
}

// WebView Metrics are sampled based on GUID value. The sample rate must not
// exceed 10%; see https://crbug.com/839646 for more info.
// TODO(paulmiller) Sample with Finch, once we have Finch.
bool IsInSample(const std::string& client_id) {
  // client_id comes from base::GenerateGUID(), so its value is random/uniform,
  // except for a few bit positions with fixed values, and some hyphens. Rather
  // than separating the random payload from the fixed bits, just hash the whole
  // thing, to produce a new random/~uniform value.
  uint32_t hash = base::PersistentHash(client_id);

  // Since hashing is ~uniform, the chance that the value falls in the bottom
  // 2% (1/50th) of possible values is 2%.
  return hash < UINT32_MAX / 50u;
}

}  // namespace

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_lazy_instance_.Pointer();
}

void AwMetricsServiceClient::LoadOrCreateClientId() {
  // This function should only be called once at start up.
  DCHECK_NE(g_client_id.Get().length(), kGuidSize);

  // UMA uses randomly-generated GUIDs (globally unique identifiers) to
  // anonymously identify logs. Every WebView-using app on every device
  // is given a GUID, stored in this file in the app's data directory.
  base::FilePath user_data_dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir)) {
    LOG(ERROR) << "Failed to get app data directory for Android WebView";

    // Generate a 1-time GUID so metrics can still be collected
    g_client_id.Get() = base::GenerateGUID();
    return;
  }

  const base::FilePath guid_file_path =
      user_data_dir.Append(FILE_PATH_LITERAL(kGuidFileName));

  // Try to get an existing GUID.
  if (base::ReadFileToStringWithMaxSize(guid_file_path, &g_client_id.Get(),
                                        kGuidSize)) {
    if (base::IsValidGUID(g_client_id.Get()))
      return;
    LOG(ERROR) << "Overwriting invalid GUID";
  }

  // We must write a new GUID.
  g_client_id.Get() = base::GenerateGUID();
  if (!base::WriteFile(guid_file_path, g_client_id.Get().c_str(),
                       g_client_id.Get().size())) {
    // If writing fails, proceed anyway with the new GUID. It won't be persisted
    // to the next run, but we can still collect metrics with this 1-time GUID.
    LOG(ERROR) << "Failed to write new GUID";
  }
}

std::string AwMetricsServiceClient::GetClientId() {
  // This function should only be called if LoadOrCreateClientId() was
  // previously called.
  DCHECK_EQ(g_client_id.Get().length(), kGuidSize);

  return g_client_id.Get();
}

void AwMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(pref_service_ == nullptr);  // Initialize should only happen once.
  pref_service_ = pref_service;

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service_, this, base::string16(),
      base::BindRepeating(&StoreClientInfo),
      base::BindRepeating(&LoadClientInfo));

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AwMetricsServiceClient::LoadOrCreateClientId),
      base::BindOnce(&AwMetricsServiceClient::InitializeWithClientId,
                     base::Unretained(this)));
}

void AwMetricsServiceClient::InitializeWithClientId() {
  // The client ID must be loaded (because LoadOrCreateClientId() finished), but
  // not yet stored in prefs.
  DCHECK_EQ(g_client_id.Get().length(), kGuidSize);
  DCHECK(!pref_service_->HasPrefPath(metrics::prefs::kMetricsClientID));

  pref_service_->SetString(metrics::prefs::kMetricsClientID, g_client_id.Get());
  in_sample_ = IsInSample(g_client_id.Get());

  metrics_service_.reset(new ::metrics::MetricsService(
      metrics_state_manager_.get(), this, pref_service_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::NetworkMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::GPUMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

  metrics_service_->InitializeMetricsRecordingState();

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwMetricsServiceClient_nativeInitialized(env);
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
AwMetricsServiceClient::CreateLowEntropyProvider() {
  return metrics_state_manager_->CreateLowEntropyProvider();
}

bool AwMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return consent_;
}

bool AwMetricsServiceClient::IsReportingEnabled() const {
  return consent_ && in_sample_;
}

void AwMetricsServiceClient::SetHaveMetricsConsent(bool consent) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  consent_ = consent;
  // Receiving this call is the last step in determining whether metrics should
  // be enabled; if so, start metrics. There's no need for a matching Stop()
  // call, since SetHaveMetricsConsent(false) never happens, and WebView has no
  // shutdown sequence.
  if (IsReportingEnabled()) {
    metrics_service_->Start();
  }
}

metrics::MetricsService* AwMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

// In Chrome, UMA and Breakpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In WebView, UMA and Breakpad are
// independent, so this is a no-op.
void AwMetricsServiceClient::SetMetricsClientId(const std::string& client_id) {}

int32_t AwMetricsServiceClient::GetProduct() {
  return ::metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

std::string AwMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool AwMetricsServiceClient::GetBrand(std::string* brand_code) {
  // WebView doesn't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel AwMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(version_info::GetChannel());
}

std::string AwMetricsServiceClient::GetVersionString() {
  return version_info::GetVersionNumber();
}

void AwMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  done_callback.Run();
}

std::unique_ptr<metrics::MetricsLogUploader>
AwMetricsServiceClient::CreateUploader(
    base::StringPiece server_url,
    base::StringPiece insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  // |server_url|, |insecure_server_url| and |mime_type| are unused because
  // WebView uses the platform logging mechanism instead of the normal UMA
  // server.
  return std::unique_ptr<::metrics::MetricsLogUploader>(
      new AwMetricsLogUploader(on_upload_complete));
}

base::TimeDelta AwMetricsServiceClient::GetStandardUploadInterval() {
  // The platform logging mechanism is responsible for upload frequency; this
  // just specifies how frequently to provide logs to the platform.
  return base::TimeDelta::FromMinutes(kUploadIntervalMinutes);
}

AwMetricsServiceClient::AwMetricsServiceClient()
    : pref_service_(nullptr),
      consent_(false),
      in_sample_(false) {}

AwMetricsServiceClient::~AwMetricsServiceClient() {}

// static
void JNI_AwMetricsServiceClient_SetHaveMetricsConsent(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jboolean consent) {
  g_lazy_instance_.Pointer()->SetHaveMetricsConsent(consent);
}

}  // namespace android_webview
