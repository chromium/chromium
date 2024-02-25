// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"

#include "base/run_loop.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/history/test_print_job_history_service_observer.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/version.h"
#include "chromeos/crosapi/mojom/printing_metrics.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace extensions {

namespace {

constexpr char kTitle[] = "title";

constexpr char kUpdateManifestPath[] =
    "/extensions/api_test/printing_metrics/update_manifest.xml";
// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
constexpr char kTestExtensionID[] = "cmgkkmeeoiceijkpmaabbmpgnkpaaela";

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<KeyedService> BuildTestCupsPrintJobManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class PrintingMetricsApiTest : public ExtensionApiTest {
 public:
  PrintingMetricsApiTest() {}

  PrintingMetricsApiTest(const PrintingMetricsApiTest&) = delete;
  PrintingMetricsApiTest& operator=(const PrintingMetricsApiTest&) = delete;

  ~PrintingMetricsApiTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // Init the user policy provider.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PrintingMetricsApiTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
#endif
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  const Extension* extension() {
    return ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
        kTestExtensionID);
  }

  void ForceInstallExtensionByPolicy() {
    policy_test_utils::SetUpEmbeddedTestServer(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    policy_test_utils::SetExtensionInstallForcelistPolicy(
        kTestExtensionID, embedded_test_server()->GetURL(kUpdateManifestPath),
        profile(), &policy_provider_);
    ASSERT_TRUE(extension());
  }

  void CreateAndCancelPrintJob(const std::string& job_title) {
    base::RunLoop run_loop;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::TestPrintJobHistoryServiceObserver observer(
        ash::PrintJobHistoryServiceFactory::GetForBrowserContext(
            browser()->profile()),
        run_loop.QuitClosure());

    std::unique_ptr<ash::CupsPrintJob> print_job =
        std::make_unique<ash::CupsPrintJob>(
            chromeos::Printer(), /*job_id=*/0, job_title,
            /*total_page_number=*/1,
            ::printing::PrintJob::Source::kPrintPreview,
            /*source_id=*/"", ash::printing::proto::PrintSettings());
    ash::TestCupsPrintJobManager* print_job_manager =
        static_cast<ash::TestCupsPrintJobManager*>(
            ash::CupsPrintJobManagerFactory::GetForBrowserContext(
                browser()->profile()));
    print_job_manager->CreatePrintJob(print_job.get());
    print_job_manager->CancelPrintJob(print_job.get());
#else
    GetTestController()->CreateAndCancelPrintJob(job_title,
                                                 run_loop.QuitClosure());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    run_loop.Run();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::TestController* GetTestController() {
    auto* service = chromeos::LacrosService::Get();
    if (!service->IsRegistered<crosapi::mojom::PrintingMetrics>() ||
        !service->IsAvailable<crosapi::mojom::TestController>() ||
        service->GetInterfaceVersion<crosapi::mojom::TestController>() <
            static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                                 kCreateAndCancelPrintJobMinVersion)) {
      LOG(ERROR) << "Unsupported ash version.";
      return nullptr;
    }
    return service->GetRemote<crosapi::mojom::TestController>().get();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ash::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestCupsPrintJobManager));
  }

  base::CallbackListSubscription create_services_subscription_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

IN_PROC_BROWSER_TEST_F(PrintingMetricsApiTest, GetPrintJobs) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!GetTestController() ||
      !chromeos::IsAshVersionAtLeastForTesting(base::Version({120, 0, 6079}))) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  ForceInstallExtensionByPolicy();

  CreateAndCancelPrintJob(kTitle);

  Browser* const new_browser = CreateBrowser(profile());
  SetCustomArg(kTitle);
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      new_browser, extension()->GetResourceURL("get_print_jobs.html")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(PrintingMetricsApiTest, OnPrintJobFinished) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!GetTestController()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  ForceInstallExtensionByPolicy();

  ResultCatcher catcher;
  Browser* const new_browser = CreateBrowser(profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      new_browser, extension()->GetResourceURL("on_print_job_finished.html")));

  CreateAndCancelPrintJob(kTitle);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the printingMetrics permission in the manifest and
// that such extensions don't see the chrome.printingMetrics namespace.
IN_PROC_BROWSER_TEST_F(PrintingMetricsApiTest, IsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionTest("printing_metrics",
                               {.extension_url = "api_not_available.html"},
                               {.ignore_manifest_warnings = true}));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("printing_metrics");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const extensions::Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  EXPECT_THAT(
      extension->install_warnings(),
      testing::Contains(testing::Field(
          &extensions::InstallWarning::message,
          "'printingMetrics' is not allowed for specified install location.")));
}

}  // namespace extensions
