// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SERVICES_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SERVICES_DELEGATE_ANDROID_H_

#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

class AndroidTelemetryService;

// Android ServicesDelegate implementation. Create via
// ServicesDelegate::Create().
class ServicesDelegateAndroid : public ServicesDelegate {
 public:
  explicit ServicesDelegateAndroid(
      SafeBrowsingServiceImpl* safe_browsing_service);

  ServicesDelegateAndroid(const ServicesDelegateAndroid&) = delete;
  ServicesDelegateAndroid& operator=(const ServicesDelegateAndroid&) = delete;

  ~ServicesDelegateAndroid() override;

 private:
  // ServicesDelegate:
  const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const override;
  void Initialize() override;
  void SetDatabaseManagerForTest(
      SafeBrowsingDatabaseManager* database_manager) override;
  void ShutdownServices() override;
  void RefreshState(bool enable) override;
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) override;
  void RegisterDelayedAnalysisCallback(
      DelayedAnalysisCallback callback) override;
  void AddDownloadManager(content::DownloadManager* download_manager) override;

  void StartOnUIThread(
      scoped_refptr<network::SharedURLLoaderFactory> browser_url_loader_factory,
      const V4ProtocolConfig& v4_config) override;
  void StopOnUIThread(bool shutdown) override;

  void CreateTelemetryService(Profile* profile) override;
  void RemoveTelemetryService(Profile* profile) override;

  // The telemetry service tied to the current profile.
  std::unique_ptr<AndroidTelemetryService> telemetry_service_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  // Has the database_manager been set for tests?
  bool database_manager_set_for_tests_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SERVICES_DELEGATE_ANDROID_H_
