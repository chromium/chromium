// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <jni.h>
#include <cstdint>

#include "android_webview/browser/metrics/android_metrics_provider.h"
#include "android_webview/common/aw_features.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_paths_android.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwMetricsServiceClient_jni.h"

namespace android_webview {

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 2%, based on storage concerns. We sample at a different rate than
// Chrome because we have more metrics "clients" (each app on the device counts
// as a separate client).
const int kStableSampledInRatePerMille = 20;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const int kBetaDevCanarySampledInRatePerMille = 990;

// The fraction of UMA clients for whom package name data is uploaded. This
// threshold and the corresponding privacy requirements are described in more
// detail at http://shortn/_CzfDUxTxm2 (internal document). We also have public
// documentation for metrics collection in WebView more generally (see
// https://developer.android.com/guide/webapps/webview-privacy).
//
// Do not change this constant without seeking privacy approval with the teams
// outlined in the internal document above.
const int kPackageNameLimitRatePerMille = 100;  // (10% of UMA clients)

AwMetricsServiceClient* g_aw_metrics_service_client = nullptr;

int GetBaseSampleRatePerMille() {
  // Down-sample unknown channel as a precaution in case it ends up being
  // shipped to Stable users.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return kStableSampledInRatePerMille;
  }
  return kBetaDevCanarySampledInRatePerMille;
}

}  // namespace

const base::TimeDelta kRecordAppDataDirectorySizeDelay = base::Seconds(10);

AwMetricsServiceClient::Delegate::Delegate() = default;
AwMetricsServiceClient::Delegate::~Delegate() = default;

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  DCHECK(g_aw_metrics_service_client);
  g_aw_metrics_service_client->EnsureOnValidSequence();
  return g_aw_metrics_service_client;
}

// static
void AwMetricsServiceClient::SetInstance(
    std::unique_ptr<AwMetricsServiceClient> aw_metrics_service_client) {
  DCHECK(!g_aw_metrics_service_client);
  DCHECK(aw_metrics_service_client);
  g_aw_metrics_service_client = aw_metrics_service_client.release();
  g_aw_metrics_service_client->EnsureOnValidSequence();
}

AwMetricsServiceClient::AwMetricsServiceClient(
    std::unique_ptr<Delegate> delegate)
    : time_created_(base::Time::Now()), delegate_(std::move(delegate)) {}

AwMetricsServiceClient::~AwMetricsServiceClient() = default;

int32_t AwMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

int AwMetricsServiceClient::GetSampleRatePerMille() const {
  return 1000;
}

bool AwMetricsServiceClient::ShouldApplyMetricsFiltering() const {
  bool used_to_sample_in = GetSampleBucketValue() < GetBaseSampleRatePerMille();
  return !used_to_sample_in;
}

std::string AwMetricsServiceClient::GetAppPackageNameIfLoggable() {
  AndroidMetricsServiceClient::InstallerPackageType installer_type =
      GetInstallerPackageType();
  // Always record the app package name of system apps and apps
  // from the play store
  if (installer_type == InstallerPackageType::SYSTEM_APP ||
      installer_type == InstallerPackageType::GOOGLE_PLAY_STORE) {
    return GetAppPackageName();
  }
  return std::string();
}

bool AwMetricsServiceClient::ShouldRecordPackageName() {
  return true;
}

// Used below in AwMetricsServiceClient::OnMetricsStart.
void RecordAppDataDirectorySize() {
  TRACE_EVENT_BEGIN0("android_webview", "RecordAppDataDirectorySize");
  base::TimeTicks start_time = base::TimeTicks::Now();

  base::FilePath app_data_dir;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_dir);
  int64_t bytes = base::ComputeDirectorySize(app_data_dir);
  // Record size up to 100MB
  base::UmaHistogramCounts100000("Android.WebView.AppDataDirectory.Size",
                                 bytes / 1024);

  base::UmaHistogramMediumTimes(
      "Android.WebView.AppDataDirectory.TimeToComputeSize",
      base::TimeTicks::Now() - start_time);
  TRACE_EVENT_END0("android_webview", "RecordAppDataDirectorySize");
}

void AwMetricsServiceClient::OnMetricsStart() {
  delegate_->AddWebViewAppStateObserver(this);
  if (base::FeatureList::IsEnabled(
          android_webview::features::kWebViewRecordAppDataDirectorySize) &&
      IsReportingEnabled()) {
    // Calculating directory size can be fairly expensive, so only do this when
    // we are certain that the UMA histogram will be logged to the server.
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&RecordAppDataDirectorySize),
        kRecordAppDataDirectorySizeDelay);
  }
}

void AwMetricsServiceClient::OnMetricsNotStarted() {}

int AwMetricsServiceClient::GetPackageNameLimitRatePerMille() {
  return kPackageNameLimitRatePerMille;
}

void AwMetricsServiceClient::OnAppStateChanged(
    WebViewAppStateObserver::State state) {
  // To match MetricsService's expectation,
  // - does nothing if no WebView has ever been created.
  // - starts notifying MetricsService once a WebView is created and the app
  //   is foreground.
  // - consolidates the other states other than kForeground into background.
  // - avoids the duplicated notification.
  if (state == WebViewAppStateObserver::State::kDestroyed &&
      !delegate_->HasAwContentsEverCreated()) {
    return;
  }

  bool foreground = state == WebViewAppStateObserver::State::kForeground;

  if (foreground == app_in_foreground_)
    return;

  app_in_foreground_ = foreground;
  if (app_in_foreground_) {
    GetMetricsService()->OnAppEnterForeground();
  } else {
    // TODO(crbug.com/40118864): Turn on the background recording.
    // Not recording in background, this matches Chrome's behavior.
    GetMetricsService()->OnAppEnterBackground(
        /* keep_recording_in_background = false */);
  }
}

void AwMetricsServiceClient::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  delegate_->RegisterAdditionalMetricsProviders(service);
}

// static
void AwMetricsServiceClient::RegisterMetricsPrefs(
    PrefRegistrySimple* registry) {
  RegisterPrefs(registry);
  AndroidMetricsProvider::RegisterPrefs(registry);
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
      base::Milliseconds(upload_interval_ms));
}

// static
void JNI_AwMetricsServiceClient_SetOnFinalMetricsCollectedListenerForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& listener) {
  AwMetricsServiceClient::GetInstance()
      ->SetOnFinalMetricsCollectedListenerForTesting(base::BindRepeating(
          base::android::RunRunnableAndroid,
          base::android::ScopedJavaGlobalRef<jobject>(listener)));
}

}  // namespace android_webview
