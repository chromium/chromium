// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"

#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/common/url_constants.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#endif

namespace safe_browsing {

// TestSafeBrowsingService functions:
TestSafeBrowsingService::TestSafeBrowsingService()
    : test_shared_loader_factory_(
          test_url_loader_factory_.GetSafeWeakWrapper()) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  services_delegate_ = ServicesDelegate::CreateForTest(this, this);
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

TestSafeBrowsingService::~TestSafeBrowsingService() {}

V4ProtocolConfig TestSafeBrowsingService::GetV4ProtocolConfig() const {
  if (v4_protocol_config_)
    return *v4_protocol_config_;
  return SafeBrowsingService::GetV4ProtocolConfig();
}

void TestSafeBrowsingService::UseV4LocalDatabaseManager() {
  use_v4_local_db_manager_ = true;
}

void TestSafeBrowsingService::SetUseTestUrlLoaderFactory(
    bool use_test_url_loader_factory) {
  use_test_url_loader_factory_ = use_test_url_loader_factory;
}

network::TestURLLoaderFactory*
TestSafeBrowsingService::GetTestUrlLoaderFactory() {
  DCHECK(use_test_url_loader_factory_);
  return &test_url_loader_factory_;
}

base::CallbackListSubscription TestSafeBrowsingService::RegisterStateCallback(
    const base::RepeatingClosure& callback) {
  // This override is required since TestSafeBrowsingService can be destroyed
  // before CertificateReportingService, which causes a crash due to the
  // leftover callback at destruction time.
  return {};
}

std::string TestSafeBrowsingService::serialized_download_report() {
  return serialized_download_report_;
}

void TestSafeBrowsingService::ClearDownloadReport() {
  serialized_download_report_.clear();
}

void TestSafeBrowsingService::SetDatabaseManager(
    TestSafeBrowsingDatabaseManager* database_manager) {
  SetDatabaseManagerForTest(database_manager);
}

void TestSafeBrowsingService::SetUIManager(
    TestSafeBrowsingUIManager* ui_manager) {
  ui_manager_ = ui_manager;
}

SafeBrowsingUIManager* TestSafeBrowsingService::CreateUIManager() {
  if (ui_manager_)
    return ui_manager_.get();
  return SafeBrowsingService::CreateUIManager();
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void TestSafeBrowsingService::SendDownloadReport(
    download::DownloadItem* download,
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    std::optional<bool> show_download_in_folder) {
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(report_type);
  report->set_download_verdict(
      DownloadProtectionService::GetDownloadProtectionVerdict(download));
  report->set_url(download->GetURL().spec());
  report->set_did_proceed(did_proceed);
  if (show_download_in_folder) {
    report->set_show_download_in_folder(show_download_in_folder.value());
  }

  std::string token = DownloadProtectionService::GetDownloadPingToken(download);
  if (!token.empty()) {
    report->set_token(token);
  }
  report->SerializeToString(&serialized_download_report_);
  return;
}
#endif

const scoped_refptr<SafeBrowsingDatabaseManager>&
TestSafeBrowsingService::database_manager() const {
  if (test_database_manager_)
    return test_database_manager_;
  return SafeBrowsingService::database_manager();
}

void TestSafeBrowsingService::SetV4ProtocolConfig(
    V4ProtocolConfig* v4_protocol_config) {
  v4_protocol_config_.reset(v4_protocol_config);
}
// ServicesDelegate::ServicesCreator:
bool TestSafeBrowsingService::CanCreateDatabaseManager() {
  return !use_v4_local_db_manager_;
}
#if BUILDFLAG(FULL_SAFE_BROWSING)
bool TestSafeBrowsingService::CanCreateDownloadProtectionService() {
  return false;
}
#endif
bool TestSafeBrowsingService::CanCreateIncidentReportingService() {
  return true;
}

SafeBrowsingDatabaseManager* TestSafeBrowsingService::CreateDatabaseManager() {
  DCHECK(!use_v4_local_db_manager_);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return new TestSafeBrowsingDatabaseManager(
      content::GetUIThreadTaskRunner({}));
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
DownloadProtectionService*
TestSafeBrowsingService::CreateDownloadProtectionService() {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif
IncidentReportingService*
TestSafeBrowsingService::CreateIncidentReportingService() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return new IncidentReportingService(nullptr);
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

scoped_refptr<network::SharedURLLoaderFactory>
TestSafeBrowsingService::GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  if (use_test_url_loader_factory_)
    return test_shared_loader_factory_;
  return SafeBrowsingService::GetURLLoaderFactory(browser_context);
}

// TestSafeBrowsingServiceFactory functions:
TestSafeBrowsingServiceFactory::TestSafeBrowsingServiceFactory()
    : test_safe_browsing_service_(nullptr), use_v4_local_db_manager_(false) {}

TestSafeBrowsingServiceFactory::~TestSafeBrowsingServiceFactory() {}

SafeBrowsingService*
TestSafeBrowsingServiceFactory::CreateSafeBrowsingService() {
  // Instantiate TestSafeBrowsingService.
  test_safe_browsing_service_ = new TestSafeBrowsingService();
  // Plug-in test member clases accordingly.
  if (use_v4_local_db_manager_)
    test_safe_browsing_service_->UseV4LocalDatabaseManager();
  if (test_ui_manager_)
    test_safe_browsing_service_->SetUIManager(test_ui_manager_.get());
  if (test_database_manager_) {
    test_safe_browsing_service_->SetDatabaseManager(
        test_database_manager_.get());
  }
  return test_safe_browsing_service_;
}

TestSafeBrowsingService*
TestSafeBrowsingServiceFactory::test_safe_browsing_service() {
  return test_safe_browsing_service_;
}

void TestSafeBrowsingServiceFactory::SetTestUIManager(
    TestSafeBrowsingUIManager* ui_manager) {
  test_ui_manager_ = ui_manager;
}

void TestSafeBrowsingServiceFactory::SetTestDatabaseManager(
    TestSafeBrowsingDatabaseManager* database_manager) {
  test_database_manager_ = database_manager;
}
void TestSafeBrowsingServiceFactory::UseV4LocalDatabaseManager() {
  use_v4_local_db_manager_ = true;
}

// TestSafeBrowsingUIManager functions:
TestSafeBrowsingUIManager::TestSafeBrowsingUIManager()
    : SafeBrowsingUIManager(
          std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
          std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
          GURL(chrome::kChromeUINewTabURL)) {}

TestSafeBrowsingUIManager::TestSafeBrowsingUIManager(
    std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory)
    : SafeBrowsingUIManager(
          std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
          std::move(blocking_page_factory),
          GURL(chrome::kChromeUINewTabURL)) {}

void TestSafeBrowsingUIManager::SendThreatDetails(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  std::string serialized;
  report->SerializeToString(&serialized);
  details_.push_back(serialized);
}

std::list<std::string>* TestSafeBrowsingUIManager::GetThreatDetails() {
  return &details_;
}

TestSafeBrowsingUIManager::~TestSafeBrowsingUIManager() {}
}  // namespace safe_browsing
