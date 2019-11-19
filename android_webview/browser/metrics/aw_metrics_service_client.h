// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;

namespace metrics {
class MetricsStateManager;
}

namespace android_webview {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones. Visible for testing.
enum class BackfillInstallDate {
  kValidInstallDatePref = 0,
  kCouldNotGetPackageManagerInstallDate = 1,
  kPersistedPackageManagerInstallDate = 2,
  kMaxValue = kPersistedPackageManagerInstallDate,
};

// AwMetricsServiceClient is a singleton which manages WebView metrics
// collection.
//
// Metrics should be enabled iff all these conditions are met:
//  - The user has not opted out (controlled by GMS).
//  - The app has not opted out (controlled by manifest tag).
//  - This client is in the 2% sample (controlled by client ID hash).
// The first two are recorded in |user_consent_| and |app_consent_|, which are
// set by SetHaveMetricsConsent(). The last is recorded in |is_in_sample_|.
//
// Metrics are pseudonymously identified by a randomly-generated "client ID".
// WebView stores this in prefs, written to the app's data directory. There's a
// different such directory for each user, for each app, on each device. So the
// ID should be unique per (device, app, user) tuple.
//
// To avoid the appearance that we're doing anything sneaky, the client ID
// should only be created and retained when neither the user nor the app have
// opted out. Otherwise, the presence of the ID could give the impression that
// metrics were being collected.
//
// WebView metrics set up happens like so:
//
//   startup
//      │
//      ├────────────┐
//      │            ▼
//      │         query GMS for consent
//      ▼            │
//   Initialize()    │
//      │            ▼
//      │         SetHaveMetricsConsent()
//      │            │
//      │ ┌──────────┘
//      ▼ ▼
//   MaybeStartMetrics()
//      │
//      ▼
//   MetricsService::Start()
//
// All the named functions in this diagram happen on the UI thread. Querying GMS
// happens in the background, and the result is posted back to the UI thread, to
// SetHaveMetricsConsent(). Querying GMS is slow, so SetHaveMetricsConsent()
// typically happens after Initialize(), but it may happen before.
//
// Each path sets a flag, |init_finished_| or |set_consent_finished_|, to show
// that path has finished, and then calls MaybeStartMetrics(). When
// MaybeStartMetrics() is called the first time, it sees only one flag is true,
// and does nothing. When MaybeStartMetrics() is called the second time, it
// decides whether to start metrics.
//
// If consent was granted, MaybeStartMetrics() determines sampling by hashing
// the client ID (generating a new ID if there was none). If this client is in
// the sample, it then calls MetricsService::Start(). If consent was not
// granted, MaybeStartMetrics() instead clears the client ID, if any.
class AwMetricsServiceClient : public metrics::MetricsServiceClient,
                               public metrics::EnabledStateProvider,
                               public content::NotificationObserver {
  friend class base::NoDestructor<AwMetricsServiceClient>;

 public:
  static AwMetricsServiceClient* GetInstance();

  AwMetricsServiceClient();
  ~AwMetricsServiceClient() override;

  void Initialize(PrefService* pref_service);
  void SetHaveMetricsConsent(bool user_consent, bool app_consent);
  void SetFastStartupForTesting(bool fast_startup_for_testing);
  void SetUploadIntervalForTesting(const base::TimeDelta& upload_interval);
  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateLowEntropyProvider();

  // metrics::EnabledStateProvider
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  // metrics::MetricsServiceClient
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  bool ShouldStartUpFastForTesting() const override;

  // Gets the embedding app's package name if it's OK to log. Otherwise, this
  // returns the empty string.
  std::string GetAppPackageName() override;

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  // Returns the metrics sampling rate, to be used by IsInSample(). This is a
  // double in the non-inclusive range (0.00, 1.00). Virtual for testing.
  virtual double GetSampleRate();

  // Determines if the client is within the random sample of clients for which
  // we log metrics. If this returns false, AwMetricsServiceClient should
  // indicate reporting is disabled. Sampling is due to storage/bandwidth
  // considerations. Virtual for testing.
  virtual bool IsInSample();

  // Prefer calling the IsInSample() which takes no arguments. Virtual for
  // testing.
  virtual bool IsInSample(uint32_t value);

  // Determines if the embedder app is the type of app for which we may log the
  // package name. If this returns false, GetAppPackageName() must return empty
  // string. Virtual for testing.
  virtual bool CanRecordPackageNameForAppType();

  // Determines if this client falls within the group for which it's acceptable
  // to include the embedding app's package name. If this returns false,
  // GetAppPackageName() must return the empty string (for
  // privacy/fingerprintability reasons). Virtual for testing.
  virtual bool IsInPackageNameSample();

  // Prefer calling the IsInPackageNameSample() which takes no arguments.
  // Virtual for testing.
  virtual bool IsInPackageNameSample(uint32_t value);

 private:
  void MaybeStartMetrics();
  void RegisterForNotifications();

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;
  content::NotificationRegistrar registrar_;
  PrefService* pref_service_ = nullptr;
  bool init_finished_ = false;
  bool set_consent_finished_ = false;
  bool user_consent_ = false;
  bool app_consent_ = false;
  bool is_in_sample_ = false;
  bool fast_startup_for_testing_ = false;

  // When non-zero, this overrides the default value in
  // GetStandardUploadInterval().
  base::TimeDelta overridden_upload_interval_;

  // AwMetricsServiceClient may be created before the UI thread is promoted to
  // BrowserThread::UI. Use |sequence_checker_| to enforce that the
  // AwMetricsServiceClient is used on a single thread.
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_
