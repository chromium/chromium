// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/webui/print_management/url_constants.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/callback_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/history/print_job_database.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class FakePrintJobHistoryService : public PrintJobHistoryService {
 public:
  FakePrintJobHistoryService() = default;
  ~FakePrintJobHistoryService() override = default;

  FakePrintJobHistoryService(const FakePrintJobHistoryService&) = delete;
  FakePrintJobHistoryService& operator=(const FakePrintJobHistoryService&) =
      delete;

  void GetPrintJobs(PrintJobDatabase::GetPrintJobsCallback callback) override {
    std::vector<printing::proto::PrintJobInfo> print_jobs;

    // On the second opening of Print Management, a print job will show.
    if (!is_first_get_print_jobs_call_) {
      print_jobs.emplace_back(ConstructPrintJobInfo("My Print Job"));
    }

    is_first_get_print_jobs_call_ = false;
    std::move(callback).Run(/*success=*/true, std::move(print_jobs));
  }

  void DeleteAllPrintJobs(
      PrintJobDatabase::DeletePrintJobsCallback callback) override {}

  printing::proto::PrintJobInfo ConstructPrintJobInfo(
      const std::string& title) {
    printing::proto::PrintJobInfo print_job_info;
    print_job_info.set_title(title);
    print_job_info.set_creation_time(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    return print_job_info;
  }

 private:
  bool is_first_get_print_jobs_call_ = true;
};

std::unique_ptr<KeyedService> BuildPrintJobHistoryService(
    content::BrowserContext* context) {
  return std::make_unique<FakePrintJobHistoryService>();
}

class PrintManagementInteractiveUiTest : public InteractiveAshTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  ash::PrintJobHistoryServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(&BuildPrintJobHistoryService));
                }));
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking.
    SetupContextWidget();

    // Ensure the OS Settings and Print Management system web apps (SWA) are
    // installed.
    InstallSystemApps();
  }

  ui::test::InteractiveTestApi::MultiStep LaunchPrintManagementApp(
      const ui::ElementIdentifier& id) {
    return Steps(
        Log("Opening Print Management app"),
        InstrumentNextTab(id, AnyBrowser()), Do([&]() {
          CreateBrowserWindow(GURL(kChromeUIPrintManagementAppUrl));
        }),
        WaitForShow(id), Log("Waiting for Print Management app to load"),
        WaitForWebContentsReady(id, GURL(kChromeUIPrintManagementAppUrl)));
  }

  auto ClosePrinterSettings() {
    return Do([]() {
      // Printer settings is opened last so it'll be the last active browser.
      ASSERT_FALSE(BrowserList::GetInstance()->empty());
      chrome::CloseWindow(BrowserList::GetInstance()->GetLastActive());
    });
  }

  auto ReloadPrintManagement() {
    return Do([]() {
      // The test always starts from an empty state so the Print Management app
      // will always be the first browser.
      ASSERT_FALSE(BrowserList::GetInstance()->empty());
      chrome::Reload(BrowserList::GetInstance()->get(0),
                     WindowOpenDisposition::CURRENT_TAB);
    });
  }

 private:
  // Used for substituting the fake Print Job History keyed service during
  // startup.
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(PrintManagementInteractiveUiTest,
                       OpenPrinterSettingsFromPrintManagement) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrintManagementWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstPrinterSettingsWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondPrintManagementWebContentsId);

  const DeepQuery kBodyManagePrintersButton{
      "print-management", "div > div.data-container > printer-setup-info",
      "div > cr-button"};

  const DeepQuery kHeaderManagePrintersButton{
      "print-management",
      "#managePrinters",
  };

  RunTestSequence(
      LaunchPrintManagementApp(kPrintManagementWebContentsId),
      WaitForElementExists(kPrintManagementWebContentsId,
                           kBodyManagePrintersButton),
      InstrumentNextTab(kFirstPrinterSettingsWebContentsId, AnyBrowser()),
      ClickElement(kPrintManagementWebContentsId, kBodyManagePrintersButton),
      Log("Opening Printer settings from empty state"),
      WaitForShow(kFirstPrinterSettingsWebContentsId),
      WaitForWebContentsReady(
          kFirstPrinterSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrintingDetailsSubpagePath)),
      ClosePrinterSettings(), WaitForHide(kFirstPrinterSettingsWebContentsId),
      ReloadPrintManagement(),
      Log("Waiting for the Print Management app to reload"),
      WaitForHide(kPrintManagementWebContentsId),
      WaitForShow(kPrintManagementWebContentsId),
      Log("Waiting for the Manage Printers button to show"),
      WaitForElementExists(kPrintManagementWebContentsId,
                           kHeaderManagePrintersButton),
      InstrumentNextTab(kSecondPrintManagementWebContentsId, AnyBrowser()),
      ClickElement(kPrintManagementWebContentsId, kHeaderManagePrintersButton),
      Log("Waiting for the Printer settings to launch after clicking the "
          "Manage Printers button"),
      WaitForShow(kSecondPrintManagementWebContentsId),
      WaitForWebContentsReady(
          kSecondPrintManagementWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrintingDetailsSubpagePath)));
}

}  // namespace
}  // namespace ash
