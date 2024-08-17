// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/ash/scanning/scan_service.h"
#include "chrome/browser/ash/scanning/scan_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

constexpr char kScanningUrl[] = "chrome://scanning";

// Scan settings.
constexpr char kFirstTestScannerName[] = "Test Scanner 1";
constexpr char kDocumentSourceName[] = "Flatbed";
constexpr uint32_t kFirstResolution = 75;
constexpr uint32_t kSecondResolution = 150;

// Kombucha helpers.
constexpr char kClickFn[] = "e => e.click()";

lorgnette::DocumentSource CreateLorgnetteDocumentSource() {
  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SOURCE_PLATEN);
  source.set_name(kDocumentSourceName);
  source.add_color_modes(lorgnette::MODE_GRAYSCALE);
  source.add_resolutions(kFirstResolution);
  source.add_resolutions(kSecondResolution);
  return source;
}

lorgnette::ScannerCapabilities CreateLorgnetteScannerCapabilities() {
  lorgnette::ScannerCapabilities caps;
  *caps.add_sources() = CreateLorgnetteDocumentSource();
  return caps;
}

// Creates a new LorgnetteScannerManager for the given `context`.
std::unique_ptr<KeyedService> BuildLorgnetteScannerManager(
    content::BrowserContext* context) {
  auto manager = std::make_unique<FakeLorgnetteScannerManager>();
  manager->SetGetScannerNamesResponse({kFirstTestScannerName});
  manager->SetGetScannerCapabilitiesResponse(
      CreateLorgnetteScannerCapabilities());
  return manager;
}

class MultiPageScanIntegrationTest : public AshIntegrationTest {
 public:
  MultiPageScanIntegrationTest() {
    set_exit_when_last_browser_closes(false);
    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kTestLogin);
  }

  const DeepQuery kScanButtonQuery{
      "scanning-app",
      "cr-button#scanButton",
  };

  const DeepQuery kScanDoneSectionQuery{
      "scanning-app",
      "scan-done-section",
  };

  const DeepQuery kMultiPageCheckboxQuery{
      "scanning-app",
      "multi-page-checkbox",
      "div#checkboxDiv",
  };

  const DeepQuery kSourceSelectQuery{
      "scanning-app",
      "source-select",
      "select",
  };

  const DeepQuery kMultiPageScanningElementQuery{
      "scanning-app",
      "multi-page-scan",
  };

  const DeepQuery kScanNextPageButtonQuery{"scanning-app", "multi-page-scan",
                                           "cr-button#scanButton"};

  const DeepQuery kMultiPageScanSaveButtonQuery{
      "scanning-app", "multi-page-scan", "cr-button#saveButton"};

  auto LaunchScanningApp() {
    return Do([&]() { CreateBrowserWindow(GURL(kScanningUrl)); });
  }

  std::vector<base::FilePath> GetScannedFiles() {
    base::FileEnumerator e(
        file_manager::util::GetMyFilesFolderForProfile(GetActiveUserProfile()),
        /*recursive=*/false, base::FileEnumerator::FILES,
        FILE_PATH_LITERAL("*.pdf"));
    std::vector<base::FilePath> files;
    e.ForEach([&files](const base::FilePath& item) { files.push_back(item); });
    return files;
  }
};

IN_PROC_BROWSER_TEST_F(MultiPageScanIntegrationTest, MultiPageScan) {
  // Set up context for element tracking for InteractiveBrowserTest.
  SetupContextWidget();

  login_mixin().Login();

  // Waits for the primary user session to start.
  ash::test::WaitForPrimaryUserSessionStart();

  // Ensure the Scanning system web app (SWA) is installed.
  InstallSystemApps();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kScanAppWebContentsId);
  base::ScopedAllowBlockingForTesting allow_io;

  LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
      GetActiveUserProfile(),
      base::BindRepeating(&BuildLorgnetteScannerManager));

  RunTestSequence(
      InstrumentNextTab(kScanAppWebContentsId, AnyBrowser()),
      Log("Launching Scanning app"), LaunchScanningApp(),
      Log("Waiting for Scanning app to load"),
      WaitForWebContentsReady(kScanAppWebContentsId, GURL(kScanningUrl)),
      FocusWebContents(kScanAppWebContentsId),
      Log("Verifying that Flatbed is the selected source"),
      WaitForElementTextContains(kScanAppWebContentsId, kSourceSelectQuery,
                                 "Flatbed"),
      Log("Toggling multi page scanning checkbox"),
      ExecuteJsAt(kScanAppWebContentsId, kMultiPageCheckboxQuery, kClickFn),
      Log("Clicking scan button"),
      ExecuteJsAt(kScanAppWebContentsId, kScanButtonQuery, kClickFn),
      Log("Verifying that multi page scan UI is shown"),
      WaitForElementExists(kScanAppWebContentsId,
                           kMultiPageScanningElementQuery),
      Log("Verifying that no file has been created yet"),
      Check([this]() { return GetScannedFiles().size() == 0u; }),
      Log("Clicking scan next page button"),
      ExecuteJsAt(kScanAppWebContentsId, kScanNextPageButtonQuery, kClickFn),
      Log("Clicking save button"),
      ExecuteJsAt(kScanAppWebContentsId, kMultiPageScanSaveButtonQuery,
                  kClickFn),
      Check([this]() {
        const auto files = GetScannedFiles();
        // PDFs do not have stable contents (e.g. metadata changes), so do not
        // compare to a golden file. If the file was written, we assume its
        // contents are valid.
        return files.size() == 1u && base::PathExists(files.front());
      }));
}

}  // namespace

}  // namespace ash
