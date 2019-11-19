// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <memory>

#include "android_webview/browser/metrics/aw_metrics_log_uploader.h"
#include "android_webview/browser_jni_headers/AwMetricsServiceClient_jni.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/metrics/android_metrics_provider.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace android_webview {

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 2%, based on storage concerns. We sample at a different rate than
// Chrome because we have more metrics "clients" (each app on the device counts
// as a separate client).
const double kStableSampledInRate = 0.02;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const double kBetaDevCanarySampledInRate = 0.99;

// As a mitigation to preserve use privacy, the privacy team has asked that we
// upload package name with no more than 10% of UMA records. This is to mitigate
// fingerprinting for users on low-usage applications (if an app only has a
// a small handful of users, there's a very good chance many of them won't be
// uploading UMA records due to sampling). Do not change this constant without
// consulting with the privacy team.
const double kPackageNameLimitRate = 0.10;

// Callbacks for metrics::MetricsStateManager::Create. Store/LoadClientInfo
// allow Windows Chrome to back up ClientInfo. They're no-ops for WebView.

void StoreClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadClientInfo() {
  std::unique_ptr<metrics::ClientInfo> client_info;
  return client_info;
}

bool UintFallsInBottomPercentOfValues(uint32_t value, double percent) {
  DCHECK_GT(percent, 0);
  DCHECK_LT(percent, 1.00);

  // Since hashing is ~uniform, the chance that the value falls in the bottom
  // X% of possible values is X%. UINT32_MAX fits within the range of integers
  // that can be expressed precisely by a 64-bit double. Casting back to a
  // uint32_t means we can determine if the value falls within the bottom X%,
  // within a 1/UINT32_MAX error margin.
  uint32_t value_threshold =
      static_cast<uint32_t>(static_cast<double>(UINT32_MAX) * percent);
  return value < value_threshold;
}

std::unique_ptr<metrics::MetricsService> CreateMetricsService(
    metrics::MetricsStateManager* state_manager,
    metrics::MetricsServiceClient* client,
    PrefService* prefs) {
  auto service =
      std::make_unique<metrics::MetricsService>(state_manager, client, prefs);
  service->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));
  service->RegisterMetricsProvider(
      std::make_unique<metrics::AndroidMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());
  service->InitializeMetricsRecordingState();
  return service;
}

// Queries the system for the app's first install time and uses this in the
// kInstallDate pref. Must be called before created a MetricsStateManager.
// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
void PopulateSystemInstallDateIfNecessary(PrefService* prefs) {
  int64_t install_date = prefs->GetInt64(metrics::prefs::kInstallDate);
  if (install_date > 0) {
    // kInstallDate appears to be valid (common case). Finish early as an
    // optimization to avoid a JNI call below.
    base::UmaHistogramEnumeration("Android.WebView.Metrics.BackfillInstallDate",
                                  BackfillInstallDate::kValidInstallDatePref);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t system_install_date =
      Java_AwMetricsServiceClient_getAppInstallTime(env);
  if (system_install_date < 0) {
    // Could not figure out install date from the system. Let the
    // MetricsStateManager set this pref to its best guess for a reasonable
    // time.
    base::UmaHistogramEnumeration(
        "Android.WebView.Metrics.BackfillInstallDate",
        BackfillInstallDate::kCouldNotGetPackageManagerInstallDate);
    return;
  }

  base::UmaHistogramEnumeration(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kPersistedPackageManagerInstallDate);
  prefs->SetInt64(metrics::prefs::kInstallDate, system_install_date);
}

}  // namespace

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  static base::NoDestructor<AwMetricsServiceClient> client;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client.get()->sequence_checker_);
  return client.get();
}

AwMetricsServiceClient::AwMetricsServiceClient() = default;
AwMetricsServiceClient::~AwMetricsServiceClient() = default;

void AwMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_finished_);

  pref_service_ = pref_service;

  PopulateSystemInstallDateIfNecessary(pref_service_);
  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service_, this, base::string16(),
      base::BindRepeating(&StoreClientInfo),
      base::BindRepeating(&LoadClientInfo));

  init_finished_ = true;
  MaybeStartMetrics();
}

void AwMetricsServiceClient::MaybeStartMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Treat the debugging flag the same as user consent because the user set it,
  // but keep app_consent_ separate so we never persist data from an opted-out
  // app.
  bool user_consent_or_flag = user_consent_ || IsMetricsReportingForceEnabled();
  if (init_finished_ && set_consent_finished_) {
    if (app_consent_ && user_consent_or_flag) {
      metrics_service_ = CreateMetricsService(metrics_state_manager_.get(),
                                              this, pref_service_);
      // Register for notifications so we can detect when the user or app are
      // interacting with WebView. We use these as signals to wake up the
      // MetricsService.
      RegisterForNotifications();
      metrics_state_manager_->ForceClientIdCreation();
      is_in_sample_ = IsInSample();
      if (IsReportingEnabled()) {
        // WebView has no shutdown sequence, so there's no need for a matching
        // Stop() call.
        metrics_service_->Start();
      }
    } else {
      pref_service_->ClearPref(metrics::prefs::kMetricsClientID);
    }
  }
}

void AwMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
}

void AwMetricsServiceClient::SetHaveMetricsConsent(bool user_consent,
                                                   bool app_consent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_consent_finished_ = true;
  user_consent_ = user_consent;
  app_consent_ = app_consent;
  MaybeStartMetrics();
}

void AwMetricsServiceClient::SetFastStartupForTesting(
    bool fast_startup_for_testing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fast_startup_for_testing_ = fast_startup_for_testing;
}

void AwMetricsServiceClient::SetUploadIntervalForTesting(
    const base::TimeDelta& upload_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overridden_upload_interval_ = upload_interval;
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
AwMetricsServiceClient::CreateLowEntropyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metrics_state_manager_->CreateLowEntropyProvider();
}

bool AwMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_consent_ && app_consent_;
}

bool AwMetricsServiceClient::IsReportingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_consent_)
    return false;
  return IsMetricsReportingForceEnabled() ||
         (EnabledStateProvider::IsReportingEnabled() && is_in_sample_);
}

metrics::MetricsService* AwMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will be null if initialization hasn't finished, or if metrics
  // collection is disabled.
  return metrics_service_.get();
}

// In Chrome, UMA and Breakpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In WebView, UMA and Breakpad are
// independent, so this is a no-op.
void AwMetricsServiceClient::SetMetricsClientId(const std::string& client_id) {}

int32_t AwMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

std::string AwMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool AwMetricsServiceClient::GetBrand(std::string* brand_code) {
  // WebView doesn't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel AwMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(version_info::android::GetChannel());
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
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  // |server_url|, |insecure_server_url|, and |mime_type| are unused because
  // WebView sends metrics to the platform logging mechanism rather than to
  // Chrome's metrics server.
  return std::make_unique<AwMetricsLogUploader>(on_upload_complete);
}

base::TimeDelta AwMetricsServiceClient::GetStandardUploadInterval() {
  // In WebView, metrics collection (when we batch up all logged histograms into
  // a ChromeUserMetricsExtension proto) and metrics uploading (when the proto
  // goes to the server) happen separately.
  //
  // This interval controls the metrics collection rate, so we choose the
  // standard upload interval to make sure we're collecting metrics consistently
  // with Chrome for Android. The metrics uploading rate for WebView is
  // controlled by the platform logging mechanism. Since this mechanism has its
  // own logic for rate-limiting on cellular connections, we disable the
  // component-layer logic.
  if (!overridden_upload_interval_.is_zero()) {
    return overridden_upload_interval_;
  }
  return metrics::GetUploadInterval(false /* use_cellular_upload_interval */);
}

bool AwMetricsServiceClient::ShouldStartUpFastForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fast_startup_for_testing_;
}

std::string AwMetricsServiceClient::GetAppPackageName() {
  if (IsInPackageNameSample() && CanRecordPackageNameForAppType()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jstring> j_app_name =
        Java_AwMetricsServiceClient_getAppPackageName(env);
    if (j_app_name)
      return ConvertJavaStringToUTF8(env, j_app_name);
  }
  return std::string();
}

void AwMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      if (base::FeatureList::IsEnabled(features::kWebViewWakeMetricsService))
        metrics_service_->OnApplicationNotIdle();
      break;
    default:
      NOTREACHED();
  }
}

// WebView metrics are sampled at (possibly) different rates depending on
// channel, based on the client ID. Sampling is hard-coded (rather than
// controlled via variations, as in Chrome) because:
// - WebView is slow to download the variations seed and propagate it to each
//   app, so we'd miss metrics from the first few runs of each app.
// - WebView uses the low-entropy source for all studies, so there would be
//   crosstalk between the metrics sampling study and all other studies.
double AwMetricsServiceClient::GetSampleRate() {
  double sampled_in_rate = kBetaDevCanarySampledInRate;

  // Down-sample unknown channel as a precaution in case it ends up being
  // shipped to Stable users.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    sampled_in_rate = kStableSampledInRate;
  }
  return sampled_in_rate;
}

bool AwMetricsServiceClient::IsInSample() {
  // Called in MaybeStartMetrics(), after metrics_service_ is created.
  return IsInSample(base::PersistentHash(metrics_service_->GetClientId()));
}

bool AwMetricsServiceClient::IsInSample(uint32_t value) {
  return UintFallsInBottomPercentOfValues(value, GetSampleRate());
}

bool AwMetricsServiceClient::CanRecordPackageNameForAppType() {
  // Check with Java side, to see if it's OK to log the package name for this
  // type of app (see Java side for the specific requirements).
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwMetricsServiceClient_canRecordPackageNameForAppType(env);
}

bool AwMetricsServiceClient::IsInPackageNameSample() {
  // Check if this client falls within the group for which it's acceptable to
  // log package name. This guarantees we enforce the privacy requirement
  // because we never log package names for more than kPackageNameLimitRate
  // percent of clients. We'll actually log package name for less than this,
  // because we also filter out packages for certain types of apps (see
  // CanRecordPackageNameForAppType()).
  return IsInPackageNameSample(
      base::PersistentHash(metrics_service_->GetClientId()));
}

bool AwMetricsServiceClient::IsInPackageNameSample(uint32_t value) {
  return UintFallsInBottomPercentOfValues(value, kPackageNameLimitRate);
}

// static
void JNI_AwMetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                      jboolean user_consent,
                                                      jboolean app_consent) {
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(user_consent,
                                                               app_consent);
}

// static
void JNI_AwMetricsServiceClient_SetFastStartupForTesting(
    JNIEnv* env,
    jboolean fast_startup_for_testing) {
  AwMetricsServiceClient::GetInstance()->SetFastStartupForTesting(
      fast_startup_for_testing);
}

// static
void JNI_AwMetricsServiceClient_SetUploadIntervalForTesting(
    JNIEnv* env,
    jlong upload_interval_ms) {
  AwMetricsServiceClient::GetInstance()->SetUploadIntervalForTesting(
      base::TimeDelta::FromMilliseconds(upload_interval_ms));
}

}  // namespace android_webview
