// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_SERVICE_H_

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"

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
  // SafeBrowsingService overrides
  V4ProtocolConfig GetV4ProtocolConfig() const override;

  std::string serilized_download_report();
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
  std::unique_ptr<SafeBrowsingService::StateSubscription> RegisterStateCallback(
      const base::Callback<void(void)>& callback) override;

 protected:
  // SafeBrowsingService overrides
  ~TestSafeBrowsingService() override;
  SafeBrowsingUIManager* CreateUIManager() override;
  void SendSerializedDownloadReport(const std::string& report) override;

  // ServicesDelegate::ServicesCreator:
  bool CanCreateDatabaseManager() override;
  bool CanCreateDownloadProtectionService() override;
  bool CanCreateIncidentReportingService() override;
  bool CanCreateResourceRequestDetector() override;
  bool CanCreateBinaryUploadService() override;
  SafeBrowsingDatabaseManager* CreateDatabaseManager() override;
  DownloadProtectionService* CreateDownloadProtectionService() override;
  IncidentReportingService* CreateIncidentReportingService() override;
  ResourceRequestDetector* CreateResourceRequestDetector() override;
  BinaryUploadService* CreateBinaryUploadService() override;

 private:
  std::unique_ptr<V4ProtocolConfig> v4_protocol_config_;
  std::string serialized_download_report_;
  scoped_refptr<SafeBrowsingDatabaseManager> test_database_manager_;
  bool use_v4_local_db_manager_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestSafeBrowsingService);
};

class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory();
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
  TestSafeBrowsingService* test_safe_browsing_service_;
  scoped_refptr<TestSafeBrowsingDatabaseManager> test_database_manager_;
  scoped_refptr<TestSafeBrowsingUIManager> test_ui_manager_;
  bool use_v4_local_db_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestSafeBrowsingServiceFactory);
};

// This is an implemenation of SafeBrowsingUIManager without actually
// sending report to safe browsing backend. Safe browsing reports are
// stored in strings for easy verification.
class TestSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  TestSafeBrowsingUIManager();
  explicit TestSafeBrowsingUIManager(
      const scoped_refptr<SafeBrowsingService>& service);
  void SendSerializedThreatDetails(const std::string& serialized) override;
  void SetSafeBrowsingService(SafeBrowsingService* sb_service);
  std::list<std::string>* GetThreatDetails();

 protected:
  ~TestSafeBrowsingUIManager() override;
  std::list<std::string> details_;

  DISALLOW_COPY_AND_ASSIGN(TestSafeBrowsingUIManager);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_SERVICE_H_
