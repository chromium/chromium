// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace base {
class FilePath;
}

namespace metrics {
class MetricsStateManager;
}

namespace android_webview {

// This singleton manages metrics for an app using any number of WebViews. It
// must always be used on the same thread. (Currently the UI thread is enforced,
// but it could be any thread.) This is to prevent enable/disable race
// conditions, and because MetricsService is single-threaded. Initialization is
// asynchronous; even after Initialize has returned, some methods may not be
// ready to use (see below).
class AwMetricsServiceClient : public metrics::MetricsServiceClient,
                               public metrics::EnabledStateProvider {
  friend struct base::LazyInstanceTraitsBase<AwMetricsServiceClient>;

 public:
  static AwMetricsServiceClient* GetInstance();

  // Retrieve the client ID or generate one if none exists.
  static void LoadOrCreateClientId();

  // Return the cached client id.
  static std::string GetClientId();

  void Initialize(PrefService* pref_service);

  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateLowEntropyProvider();

  // metrics::EnabledStateProvider implementation
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  // The below functions must not be called until initialization has
  // asynchronously finished.

  void SetHaveMetricsConsent(bool consent);

  // metrics::MetricsServiceClient implementation
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      base::StringPiece server_url,
      base::StringPiece insecure_server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;

 private:
  AwMetricsServiceClient();
  ~AwMetricsServiceClient() override;

  void InitializeWithClientId();

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;
  PrefService* pref_service_;
  bool consent_;    // = (user has consented) && !(app has opted out)
  bool in_sample_;  // Is this client enabled by sampling?

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_
