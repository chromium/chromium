// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/services_delegate_desktop.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {
namespace {

const char* MigrateResultToString(HashPrefixMap::MigrateResult result) {
  switch (result) {
    case HashPrefixMap::MigrateResult::kUnknown:
      return "Unknown";
    case HashPrefixMap::MigrateResult::kSuccess:
      return "Success";
    case HashPrefixMap::MigrateResult::kFailure:
      return "Failure";
    case HashPrefixMap::MigrateResult::kNotNeeded:
      return "NotNeeded";
  }
}

}  // namespace

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::Create(
    SafeBrowsingServiceImpl* safe_browsing_service) {
  return base::WrapUnique(
      new ServicesDelegateDesktop(safe_browsing_service, nullptr));
}

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::CreateForTest(
    SafeBrowsingServiceImpl* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator) {
  return base::WrapUnique(
      new ServicesDelegateDesktop(safe_browsing_service, services_creator));
}

ServicesDelegateDesktop::ServicesDelegateDesktop(
    SafeBrowsingServiceImpl* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator)
    : ServicesDelegate(safe_browsing_service, services_creator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegateDesktop::~ServicesDelegateDesktop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
}

void ServicesDelegateDesktop::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager_to_set) {
  DCHECK(!database_manager_);
  database_manager_set_for_tests_ = true;
  database_manager_ = database_manager_to_set;
}

void ServicesDelegateDesktop::ShutdownServices() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  download_service_.reset();

  incident_service_.reset();

  ServicesDelegate::ShutdownServices();
}

void ServicesDelegateDesktop::RefreshState(bool enable) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (download_service_)
    download_service_->SetEnabled(enable);
}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
ServicesDelegateDesktop::CreatePreferenceValidationDelegate(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return incident_service_->CreatePreferenceValidationDelegate(profile);
}

void ServicesDelegateDesktop::RegisterDelayedAnalysisCallback(
    DelayedAnalysisCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  incident_service_->RegisterDelayedAnalysisCallback(std::move(callback));
}

void ServicesDelegateDesktop::AddDownloadManager(
    content::DownloadManager* download_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  incident_service_->AddDownloadManager(download_manager);
}

DownloadProtectionService* ServicesDelegateDesktop::GetDownloadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return download_service_.get();
}

scoped_refptr<SafeBrowsingDatabaseManager>
ServicesDelegateDesktop::CreateDatabaseManager() {
  return V4LocalDatabaseManager::Create(
      SafeBrowsingServiceImpl::GetBaseFilename(),
      content::GetUIThreadTaskRunner({}), content::GetIOThreadTaskRunner({}),
      base::BindRepeating(
          &ServicesDelegateDesktop::GetEstimatedExtendedReportingLevel,
          base::Unretained(this)),
      base::BindOnce(&UpdateSyntheticFieldTrial));
}

DownloadProtectionService*
ServicesDelegateDesktop::CreateDownloadProtectionService() {
  return new DownloadProtectionService(safe_browsing_service_);
}

IncidentReportingService*
ServicesDelegateDesktop::CreateIncidentReportingService() {
  return new IncidentReportingService(safe_browsing_service_);
}

void ServicesDelegateDesktop::StartOnUIThread(
    scoped_refptr<network::SharedURLLoaderFactory> browser_url_loader_factory,
    const V4ProtocolConfig& v4_config) {
  database_manager_->StartOnUIThread(browser_url_loader_factory, v4_config);
}

void ServicesDelegateDesktop::StopOnUIThread(bool shutdown) {
  database_manager_->StopOnUIThread(shutdown);
}

void ServicesDelegateDesktop::OnProfileWillBeDestroyed(Profile* profile) {
  download_service_->RemovePendingDownloadRequests(profile);
}

// static
void ServicesDelegateDesktop::UpdateSyntheticFieldTrial(
    HashPrefixMap::MigrateResult result) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "SafeBrowsingMigrateResult", MigrateResultToString(result),
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

}  // namespace safe_browsing
