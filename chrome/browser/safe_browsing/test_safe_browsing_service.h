// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_SERVICE_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
struct V4ProtocolConfig;
class TestSafeBrowsingDatabaseManager;
class TestSafeBrowsingUIManager;

// TestSafeBrowsingService and its factory provides a flexible way to configure
// customized safe browsing UI manager, database manager, protocol manager,
// and etc without the need of override SafeBrowsingService in tests over and
// over again.
//
// How to configure TestSafeBrowsingService in browser tests set up?
// * When overriding SetUp():
//   (1) create an instance of TestSafeBrowsingServiceFactory (
//       e.g. test_sb_factory_),
//   (2) Set up necessary test components by calling
//       test_sb_factory_->SetTest[DatabaseManager/UIManager/...](...),
//   (3) Register TestSafeBrowsingServiceFactory
//       SafeBrowsingService::RegisterFactory(test_sb_factory_);
//   (4) InProcessBrowserTest::SetUp() or other base class SetUp() function must
//       be called at last.
// * When overriding TearDown():
//   Call base class TearDown() first then call
//   SafeBrowsingService::RegisterFactory(nullptr) to unregister
//   test_sb_factory_.
class TestSafeBrowsingService : public SafeBrowsingService,
                                public ServicesDelegate::ServicesCreator {
 public:
  TestSafeBrowsingService();

  TestSafeBrowsingService(const TestSafeBrowsingService&) = delete;
  TestSafeBrowsingService& operator=(const TestSafeBrowsingService&) = delete;

  // SafeBrowsingService overrides
  V4ProtocolConfig GetV4ProtocolConfig() const override;

  std::string serialized_download_report();
  void ClearDownloadReport();

  // In browser tests, the following setters must be called before
  // SafeBrowsingService::Initialize().
  // The preferable way to use these setters is by calling corresponding
  // TestSafeBrowsingServiceFactory::SetTest[DatabaseManager/UIManager/
  // ProtocolConfig]() before InProcessBrowserTest::SetUp() is called. Then
  // inside TestSafeBrowsingServiceFactory::CreateSafeBrowsingService(),
  // TestSafeBrowsingService instance is created, customised(by using the
  // following setters), and then initialized.
  void SetUIManager(TestSafeBrowsingUIManager* ui_manager);
  void SetDatabaseManager(TestSafeBrowsingDatabaseManager* database_manager);
  void SetV4ProtocolConfig(V4ProtocolConfig* v4_protocol_config);
  const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const override;
  void UseV4LocalDatabaseManager();

  // By default, the TestSafeBrowsing service uses a regular URLLoaderFactory.
  // This function can be used to override that behavior, exposing a
  // TestURLLoaderFactory for mocking network traffic.
  void SetUseTestUrlLoaderFactory(bool use_test_url_loader_factory);

  base::CallbackListSubscription RegisterStateCallback(
      const base::RepeatingClosure& callback) override;
  network::TestURLLoaderFactory* GetTestUrlLoaderFactory();

 protected:
  // SafeBrowsingService overrides
  ~TestSafeBrowsingService() override;
  SafeBrowsingUIManager* CreateUIManager() override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  void SendDownloadReport(
      download::DownloadItem* download,
      ClientSafeBrowsingReportRequest::ReportType report_type,
      bool did_proceed,
      std::optional<bool> show_download_in_folder) override;
#endif

  // ServicesDelegate::ServicesCreator:
  bool CanCreateDatabaseManager() override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  bool CanCreateDownloadProtectionService() override;
#endif
  bool CanCreateIncidentReportingService() override;
  SafeBrowsingDatabaseManager* CreateDatabaseManager() override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  DownloadProtectionService* CreateDownloadProtectionService() override;
#endif
  IncidentReportingService* CreateIncidentReportingService() override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* browser_context) override;

 private:
  std::unique_ptr<V4ProtocolConfig> v4_protocol_config_;
  std::string serialized_download_report_;
  scoped_refptr<SafeBrowsingDatabaseManager> test_database_manager_;
  bool use_v4_local_db_manager_ = false;
  bool use_test_url_loader_factory_ = false;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory();

  TestSafeBrowsingServiceFactory(const TestSafeBrowsingServiceFactory&) =
      delete;
  TestSafeBrowsingServiceFactory& operator=(
      const TestSafeBrowsingServiceFactory&) = delete;

  ~TestSafeBrowsingServiceFactory() override;

  // Creates test safe browsing service, and configures test UI manager,
  // database manager and so on.
  SafeBrowsingService* CreateSafeBrowsingService() override;

  TestSafeBrowsingService* test_safe_browsing_service();

  // Test UI manager, database manager and protocol config need to be set before
  // SafeBrowsingService::Initialize() is called.
  void SetTestUIManager(TestSafeBrowsingUIManager* ui_manager);
  void SetTestDatabaseManager(
      TestSafeBrowsingDatabaseManager* database_manager);

  // Be default, the TestSafeBrowsingService creates an instance of the
  // TestSafeBrowsingDatabaseManager. This function can be used to override that
  // to use the usual V4LocalDatabaseManager that's used in Chrome on Desktop.
  void UseV4LocalDatabaseManager();

 private:
  raw_ptr<TestSafeBrowsingService, DanglingUntriaged>
      test_safe_browsing_service_;
  scoped_refptr<TestSafeBrowsingDatabaseManager> test_database_manager_;
  scoped_refptr<TestSafeBrowsingUIManager> test_ui_manager_;
  bool use_v4_local_db_manager_;
};

// This is an implemenation of SafeBrowsingUIManager without actually
// sending report to safe browsing backend. Safe browsing reports are
// stored in strings for easy verification.
class TestSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  TestSafeBrowsingUIManager();
  explicit TestSafeBrowsingUIManager(
      std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory);

  TestSafeBrowsingUIManager(const TestSafeBrowsingUIManager&) = delete;
  TestSafeBrowsingUIManager& operator=(const TestSafeBrowsingUIManager&) =
      delete;

  void SendThreatDetails(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override;
  std::list<std::string>* GetThreatDetails();

 protected:
  ~TestSafeBrowsingUIManager() override;
  std::list<std::string> details_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_SERVICE_H_
