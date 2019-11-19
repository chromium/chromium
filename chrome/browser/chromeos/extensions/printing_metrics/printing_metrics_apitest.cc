// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/history/test_print_job_history_service_observer.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kUpdateManifestPath[] =
    "/extensions/api_test/printing_metrics/update_manifest.xml";
// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
constexpr char kTestExtensionID[] = "cmgkkmeeoiceijkpmaabbmpgnkpaaela";

std::unique_ptr<KeyedService> BuildTestCupsPrintJobManager(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class PrintingMetricsApiTest : public ExtensionApiTest {
 public:
  PrintingMetricsApiTest() {}
  ~PrintingMetricsApiTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // Init the user policy provider.
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(
                    &PrintingMetricsApiTest::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  policy::MockConfigurationPolicyProvider policy_provider_;

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    chromeos::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestCupsPrintJobManager));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(PrintingMetricsApiTest);
};

IN_PROC_BROWSER_TEST_F(PrintingMetricsApiTest, GetPrintJobs) {
  constexpr char kTitle[] = "title";
  const int kPagesNumber = 3;

  policy_test_utils::SetUpEmbeddedTestServer(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  policy_test_utils::SetExtensionInstallForcelistPolicy(
      kTestExtensionID, embedded_test_server()->GetURL(kUpdateManifestPath),
      profile(), &policy_provider_);

  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          kTestExtensionID);
  ASSERT_TRUE(extension);

  base::RunLoop run_loop;
  chromeos::TestPrintJobHistoryServiceObserver observer(
      chromeos::PrintJobHistoryServiceFactory::GetForBrowserContext(
          browser()->profile()),
      run_loop.QuitClosure());

  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
          ::printing::PrintJob::Source::PRINT_PREVIEW,
          /*source_id=*/"", chromeos::printing::proto::PrintSettings());
  chromeos::TestCupsPrintJobManager* print_job_manager =
      static_cast<chromeos::TestCupsPrintJobManager*>(
          chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
              browser()->profile()));
  print_job_manager->CreatePrintJob(print_job.get());
  print_job_manager->CancelPrintJob(print_job.get());
  run_loop.Run();

  Browser* const new_browser = CreateBrowser(profile());
  SetCustomArg(kTitle);
  extensions::ResultCatcher catcher;
  ui_test_utils::NavigateToURL(
      new_browser, extension->GetResourceURL("get_print_jobs.html"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the printingMetrics permission in the manifest and
// that such extensions don't see the chrome.printingMetrics namespace.
IN_PROC_BROWSER_TEST_F(PrintingMetricsApiTest, IsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionSubtest("printing_metrics", "api_not_available.html",
                                  kFlagIgnoreManifestWarnings));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("printing_metrics");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const extensions::Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ("'printingMetrics' is not allowed for specified install location.",
            extension->install_warnings()[0].message);
}

}  // namespace extensions
