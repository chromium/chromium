// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/ash/scanning/scan_service.h"
#include "chrome/browser/ash/scanning/scan_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
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
constexpr char kDocumentSourceName[] = "adf_simplex";
constexpr uint32_t kFirstResolution = 75;
constexpr uint32_t kSecondResolution = 150;

// Golden files.
constexpr char kADFGoldenFile[] = "adf_simplex_jpeg_grayscale_max_150_dpi.pdf";

// Kombucha helpers.
constexpr char kClickFn[] = "e => e.click()";

lorgnette::DocumentSource CreateLorgnetteDocumentSource() {
  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SOURCE_ADF_SIMPLEX);
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

base::FilePath GetScanningTestDataDir() {
  base::FilePath base_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
  return base_path.Append(base::FilePath("chrome/test/data/scanning"));
}

class ScanIntegrationTest : public InteractiveAshTest {
 public:
  ScanIntegrationTest() {
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

  auto LaunchScanningApp() {
    return Do([&]() { CreateBrowserWindow(GURL(kScanningUrl)); });
  }

  base::FilePath GetScannedPdfFilePath() {
    base::FileEnumerator e(
        file_manager::util::GetMyFilesFolderForProfile(GetActiveUserProfile()),
        /*recursive=*/false, base::FileEnumerator::FILES,
        FILE_PATH_LITERAL("*.pdf"));
    const auto file = e.Next();
    // Only one file should exist in the temp directory.
    CHECK(e.Next().empty());
    return file;
  }
};

// TODO(b:307385730): Add tests that select various scan settings combinations.
IN_PROC_BROWSER_TEST_F(ScanIntegrationTest, ScanWithDefaultSettings) {
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
      Log("Clicking scan button"),
      InAnyContext(EnsurePresent(kScanAppWebContentsId, kScanButtonQuery)),
      ExecuteJsAt(kScanAppWebContentsId, kScanButtonQuery, kClickFn),
      WaitForElementExists(kScanAppWebContentsId, kScanDoneSectionQuery),
      FlushEvents());
  // `adf_simplex_jpeg_grayscale_max_150_dpi.pdf` contains the expected scanned
  // PDF using the preconfigured settings for the scanner.
  const auto adf_golden_file =
      GetScanningTestDataDir().AppendASCII(kADFGoldenFile);
  EXPECT_TRUE(base::ContentsEqual(adf_golden_file, GetScannedPdfFilePath()));
}

}  // namespace

}  // namespace ash
