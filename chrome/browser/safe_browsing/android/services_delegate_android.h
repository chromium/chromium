// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SERVICES_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SERVICES_DELEGATE_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"

namespace safe_browsing {

class AndroidTelemetryService;

// Android ServicesDelegate implementation. Create via
// ServicesDelegate::Create().
class ServicesDelegateAndroid : public ServicesDelegate {
 public:
  explicit ServicesDelegateAndroid(SafeBrowsingService* safe_browsing_service);
  ~ServicesDelegateAndroid() override;

 private:
  // ServicesDelegate:
  const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const override;
  void Initialize() override;
  void InitializeCsdService(scoped_refptr<network::SharedURLLoaderFactory>
                                url_loader_factory) override;
  void SetDatabaseManagerForTest(
      SafeBrowsingDatabaseManager* database_manager) override;
  void ShutdownServices() override;
  void RefreshState(bool enable) override;
  void ProcessResourceRequest(const ResourceRequestInfo* request) override;
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) override;
  void RegisterDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) override;
  void AddDownloadManager(content::DownloadManager* download_manager) override;
  ClientSideDetectionService* GetCsdService() override;
  DownloadProtectionService* GetDownloadService() override;

  void StartOnIOThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& v4_config) override;
  void StopOnIOThread(bool shutdown) override;

  void CreateTelemetryService(Profile* profile) override;
  void RemoveTelemetryService(Profile* profile) override;

  void CreateBinaryUploadService(Profile* profile) override;
  void RemoveBinaryUploadService(Profile* profile) override;
  BinaryUploadService* GetBinaryUploadService(Profile* profile) const override;

  std::string GetSafetyNetId() const override;

  // The telemetry service tied to the current profile.
  std::unique_ptr<AndroidTelemetryService> telemetry_service_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  // Has the database_manager been set for tests?
  bool database_manager_set_for_tests_ = false;

  DISALLOW_COPY_AND_ASSIGN(ServicesDelegateAndroid);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SERVICES_DELEGATE_ANDROID_H_
