// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/delayed_analysis_callback.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"

class Profile;

namespace content {
class DownloadManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace prefs {
namespace mojom {
class TrackedPreferenceValidationDelegate;
}
}

namespace safe_browsing {

class BinaryUploadService;
class ClientSideDetectionService;
class DownloadProtectionService;
class IncidentReportingService;
class PasswordProtectionService;
class ResourceRequestDetector;
struct ResourceRequestInfo;
class SafeBrowsingService;
class SafeBrowsingDatabaseManager;
struct V4ProtocolConfig;
class VerdictCacheManager;

// Abstraction to help organize code for mobile vs full safe browsing modes.
// This helper class should be owned by a SafeBrowsingService, and it handles
// responsibilities for safe browsing service classes that may or may not exist
// for a given build config. e.g. No DownloadProtectionService on mobile.
// ServicesDelegate lives on the UI thread.
class ServicesDelegate {
 public:
  // Used for tests to override service creation. If CanCreateFooService()
  // returns true, then ServicesDelegate will use the service created by
  // CreateFooService(). If CanCreateFooService() returns false, then
  // ServicesDelegate will use its built-in service creation code.
  class ServicesCreator {
   public:
    virtual bool CanCreateDatabaseManager() = 0;
    virtual bool CanCreateDownloadProtectionService() = 0;
    virtual bool CanCreateIncidentReportingService() = 0;
    virtual bool CanCreateResourceRequestDetector() = 0;
    virtual bool CanCreateBinaryUploadService() = 0;

    // Caller takes ownership of the returned object. Cannot use std::unique_ptr
    // because services may not be implemented for some build configs.
    virtual SafeBrowsingDatabaseManager* CreateDatabaseManager() = 0;
    virtual DownloadProtectionService* CreateDownloadProtectionService() = 0;
    virtual IncidentReportingService* CreateIncidentReportingService() = 0;
    virtual ResourceRequestDetector* CreateResourceRequestDetector() = 0;
    virtual BinaryUploadService* CreateBinaryUploadService() = 0;
  };

  // Creates the ServicesDelegate using its's default ServicesCreator.
  // |safe_browsing_service| is the delegate's owner.
  static std::unique_ptr<ServicesDelegate> Create(
      SafeBrowsingService* safe_browsing_service);

  // Creates the ServicesDelegate using a custom ServicesCreator, for testing.
  static std::unique_ptr<ServicesDelegate> CreateForTest(
      SafeBrowsingService* safe_browsing_service,
      ServicesDelegate::ServicesCreator* services_creator);

  ServicesDelegate(SafeBrowsingService* safe_browsing_service,
                   ServicesCreator* services_creator);
  virtual ~ServicesDelegate();

  virtual const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const = 0;

  // Initializes internal state using the ServicesCreator.
  virtual void Initialize() = 0;

  // Creates the CSD service for the given |url_loader_factory|.
  virtual void InitializeCsdService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

  virtual void SetDatabaseManagerForTest(
      SafeBrowsingDatabaseManager* database_manager) = 0;

  // Shuts down the download service.
  virtual void ShutdownServices() = 0;

  // Handles SafeBrowsingService::RefreshState() for the provided services.
  virtual void RefreshState(bool enable) = 0;

  // See the SafeBrowsingService methods of the same name.
  virtual void ProcessResourceRequest(const ResourceRequestInfo* request) = 0;
  virtual std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) = 0;
  virtual void RegisterDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) = 0;
  virtual void AddDownloadManager(
      content::DownloadManager* download_manager) = 0;

  // Returns nullptr for any service that is not available.
  virtual ClientSideDetectionService* GetCsdService() = 0;
  virtual DownloadProtectionService* GetDownloadService() = 0;

  virtual void StartOnIOThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& v4_config) = 0;
  virtual void StopOnIOThread(bool shutdown) = 0;

  void CreatePasswordProtectionService(Profile* profile);
  void RemovePasswordProtectionService(Profile* profile);
  PasswordProtectionService* GetPasswordProtectionService(
      Profile* profile) const;

  virtual void CreateTelemetryService(Profile* profile) {}
  virtual void RemoveTelemetryService(Profile* profile) {}

  virtual void CreateVerdictCacheManager(Profile* profile);
  virtual void RemoveVerdictCacheManager(Profile* profile);
  virtual VerdictCacheManager* GetVerdictCacheManager(Profile* profile) const;

  virtual void CreateBinaryUploadService(Profile* profile) = 0;
  virtual void RemoveBinaryUploadService(Profile* profile) = 0;
  virtual BinaryUploadService* GetBinaryUploadService(
      Profile* profile) const = 0;

  virtual std::string GetSafetyNetId() const = 0;

 protected:
  // Unowned pointer
  SafeBrowsingService* const safe_browsing_service_;

  // Unowned pointer
  ServicesCreator* const services_creator_;

  // Tracks existing Profiles, and their corresponding
  // ChromePasswordProtectionService instances.
  // Accessed on UI thread.
  base::flat_map<Profile*, std::unique_ptr<ChromePasswordProtectionService>>
      password_protection_service_map_;

  // Tracks existing Profiles, and their corresponding VerdictCacheManager
  // instances. Accessed on UI thread.
  base::flat_map<Profile*, std::unique_ptr<VerdictCacheManager>>
      cache_manager_map_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_
