// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/incident_reporting/delayed_analysis_callback.h"

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

#if !BUILDFLAG(IS_ANDROID)
class DownloadProtectionService;
#endif
class IncidentReportingService;
class SafeBrowsingServiceImpl;
class SafeBrowsingDatabaseManager;
struct V4ProtocolConfig;

// Abstraction to help organize code for mobile vs full safe browsing modes.
// This helper class should be owned by a SafeBrowsingServiceImpl, and it
// handles responsibilities for safe browsing service classes that may or may
// not exist for a given build config. e.g. No DownloadProtectionService on
// mobile. ServicesDelegate lives on the UI thread.
class ServicesDelegate {
 public:
  // Used for tests to override service creation. If CanCreateFooService()
  // returns true, then ServicesDelegate will use the service created by
  // CreateFooService(). If CanCreateFooService() returns false, then
  // ServicesDelegate will use its built-in service creation code.
  class ServicesCreator {
   public:
    virtual bool CanCreateDatabaseManager() = 0;
#if !BUILDFLAG(IS_ANDROID)
    virtual bool CanCreateDownloadProtectionService() = 0;
#endif
    virtual bool CanCreateIncidentReportingService() = 0;

    // Caller takes ownership of the returned object. Cannot use std::unique_ptr
    // because services may not be implemented for some build configs.
    virtual SafeBrowsingDatabaseManager* CreateDatabaseManager() = 0;
#if !BUILDFLAG(IS_ANDROID)
    virtual DownloadProtectionService* CreateDownloadProtectionService() = 0;
#endif
    virtual IncidentReportingService* CreateIncidentReportingService() = 0;
  };

  // Creates the ServicesDelegate using its's default ServicesCreator.
  // |safe_browsing_service| is the delegate's owner.
  static std::unique_ptr<ServicesDelegate> Create(
      SafeBrowsingServiceImpl* safe_browsing_service);

  // Creates the ServicesDelegate using a custom ServicesCreator, for testing.
  static std::unique_ptr<ServicesDelegate> CreateForTest(
      SafeBrowsingServiceImpl* safe_browsing_service,
      ServicesDelegate::ServicesCreator* services_creator);

  ServicesDelegate(SafeBrowsingServiceImpl* safe_browsing_service,
                   ServicesCreator* services_creator);
  virtual ~ServicesDelegate();

  virtual const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const = 0;

  // Initializes internal state using the ServicesCreator.
  virtual void Initialize() = 0;

  virtual void SetDatabaseManagerForTest(
      SafeBrowsingDatabaseManager* database_manager) = 0;

  // Shuts down the download service.
  virtual void ShutdownServices();

  // Handles SafeBrowsingServiceImpl::RefreshState() for the provided services.
  virtual void RefreshState(bool enable) = 0;

  // See the SafeBrowsingServiceImpl methods of the same name.
  virtual std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) = 0;
  virtual void RegisterDelayedAnalysisCallback(
      DelayedAnalysisCallback callback) = 0;
  virtual void AddDownloadManager(
      content::DownloadManager* download_manager) = 0;

  // Returns nullptr for any service that is not available.
#if !BUILDFLAG(IS_ANDROID)
  virtual DownloadProtectionService* GetDownloadService() = 0;
#endif

  // Takes a SharedURLLoaderFactory from the BrowserProcess, for use in the
  // database manager.
  virtual void StartOnUIThread(
      scoped_refptr<network::SharedURLLoaderFactory> browser_url_loader_factory,
      const V4ProtocolConfig& v4_config) = 0;
  virtual void StopOnUIThread(bool shutdown) = 0;

  virtual void CreateTelemetryService(Profile* profile) {}
  virtual void RemoveTelemetryService(Profile* profile) {}

  virtual void OnProfileWillBeDestroyed(Profile* profile) {}

 protected:
  // Unowned pointer
  const raw_ptr<SafeBrowsingServiceImpl> safe_browsing_service_;

  // Unowned pointer
  const raw_ptr<ServicesCreator> services_creator_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_
