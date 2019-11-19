// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/services_delegate_desktop.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/db/v4_local_database_manager.h"
#include "components/safe_browsing/verdict_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::Create(
    SafeBrowsingService* safe_browsing_service) {
  return base::WrapUnique(
      new ServicesDelegateDesktop(safe_browsing_service, nullptr));
}

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::CreateForTest(
    SafeBrowsingService* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator) {
  return base::WrapUnique(
      new ServicesDelegateDesktop(safe_browsing_service, services_creator));
}

ServicesDelegateDesktop::ServicesDelegateDesktop(
    SafeBrowsingService* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator)
    : ServicesDelegate(safe_browsing_service, services_creator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegateDesktop::~ServicesDelegateDesktop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ServicesDelegateDesktop::InitializeCsdService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(SAFE_BROWSING_CSD)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableClientSidePhishingDetection)) {
    csd_service_ = ClientSideDetectionService::Create(url_loader_factory);
  }
#endif  // BUILDFLAG(SAFE_BROWSING_CSD)
}

ExtendedReportingLevel
ServicesDelegateDesktop::GetEstimatedExtendedReportingLevel() const {
  return safe_browsing_service_->estimated_extended_reporting_by_prefs();
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
ServicesDelegateDesktop::database_manager() const {
  return database_manager_;
}

void ServicesDelegateDesktop::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!database_manager_set_for_tests_) {
    if (services_creator_ && services_creator_->CanCreateDatabaseManager())
      database_manager_ = services_creator_->CreateDatabaseManager();
    else
      database_manager_ = CreateDatabaseManager();
  }

  download_service_.reset(
      (services_creator_ &&
       services_creator_->CanCreateDownloadProtectionService())
          ? services_creator_->CreateDownloadProtectionService()
          : CreateDownloadProtectionService());
  incident_service_.reset(
      (services_creator_ &&
       services_creator_->CanCreateIncidentReportingService())
          ? services_creator_->CreateIncidentReportingService()
          : CreateIncidentReportingService());
  resource_request_detector_.reset(
      (services_creator_ &&
       services_creator_->CanCreateResourceRequestDetector())
          ? services_creator_->CreateResourceRequestDetector()
          : CreateResourceRequestDetector());
}

void ServicesDelegateDesktop::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager_to_set) {
  DCHECK(!database_manager_);
  database_manager_set_for_tests_ = true;
  database_manager_ = database_manager_to_set;
}

void ServicesDelegateDesktop::ShutdownServices() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The IO thread is going away, so make sure the ClientSideDetectionService
  // dtor executes now since it may call the dtor of URLFetcher which relies
  // on it.
  csd_service_.reset();

  resource_request_detector_.reset();
  incident_service_.reset();

  // Delete the VerdictCacheManager instances
  cache_manager_map_.clear();

  // Delete the ChromePasswordProtectionService instances.
  password_protection_service_map_.clear();

  // Must shut down last.
  download_service_.reset();
}

void ServicesDelegateDesktop::RefreshState(bool enable) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (csd_service_)
    csd_service_->SetEnabledAndRefreshState(enable);
  if (download_service_)
    download_service_->SetEnabled(enable);
}

void ServicesDelegateDesktop::ProcessResourceRequest(
    const ResourceRequestInfo* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (resource_request_detector_)
    resource_request_detector_->ProcessResourceRequest(request);
}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
ServicesDelegateDesktop::CreatePreferenceValidationDelegate(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return incident_service_->CreatePreferenceValidationDelegate(profile);
}

void ServicesDelegateDesktop::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  incident_service_->RegisterDelayedAnalysisCallback(callback);
}

void ServicesDelegateDesktop::AddDownloadManager(
    content::DownloadManager* download_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  incident_service_->AddDownloadManager(download_manager);
}

ClientSideDetectionService* ServicesDelegateDesktop::GetCsdService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return csd_service_.get();
}

DownloadProtectionService* ServicesDelegateDesktop::GetDownloadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return download_service_.get();
}

scoped_refptr<SafeBrowsingDatabaseManager>
ServicesDelegateDesktop::CreateDatabaseManager() {
  return V4LocalDatabaseManager::Create(
      SafeBrowsingService::GetBaseFilename(),
      base::BindRepeating(
          &ServicesDelegateDesktop::GetEstimatedExtendedReportingLevel,
          base::Unretained(this)));
}

DownloadProtectionService*
ServicesDelegateDesktop::CreateDownloadProtectionService() {
  return new DownloadProtectionService(safe_browsing_service_);
}

IncidentReportingService*
ServicesDelegateDesktop::CreateIncidentReportingService() {
  return new IncidentReportingService(safe_browsing_service_);
}

ResourceRequestDetector*
ServicesDelegateDesktop::CreateResourceRequestDetector() {
  return new ResourceRequestDetector(safe_browsing_service_->database_manager(),
                                     incident_service_->GetIncidentReceiver());
}

void ServicesDelegateDesktop::StartOnIOThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& v4_config) {
  database_manager_->StartOnIOThread(url_loader_factory, v4_config);
}

void ServicesDelegateDesktop::StopOnIOThread(bool shutdown) {
  database_manager_->StopOnIOThread(shutdown);
}

void ServicesDelegateDesktop::CreateBinaryUploadService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = binary_upload_service_map_.find(profile);
  DCHECK(it == binary_upload_service_map_.end());
  std::unique_ptr<BinaryUploadService> service;
  if (services_creator_ && services_creator_->CanCreateBinaryUploadService())
    service = base::WrapUnique(services_creator_->CreateBinaryUploadService());
  else
    service = std::make_unique<BinaryUploadService>(
        safe_browsing_service_->GetURLLoaderFactory(), profile);
  binary_upload_service_map_[profile] = std::move(service);
}

void ServicesDelegateDesktop::RemoveBinaryUploadService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = binary_upload_service_map_.find(profile);
  if (it != binary_upload_service_map_.end())
    binary_upload_service_map_.erase(it);
}

BinaryUploadService* ServicesDelegateDesktop::GetBinaryUploadService(
    Profile* profile) const {
  DCHECK(profile);
  auto it = binary_upload_service_map_.find(profile);
  DCHECK(it != binary_upload_service_map_.end());
  return it->second.get();
}

std::string ServicesDelegateDesktop::GetSafetyNetId() const {
  NOTREACHED() << "Only implemented on Android";
  return "";
}

}  // namespace safe_browsing
