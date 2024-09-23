// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_service.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "ash/webui/scanning/scanning_uma.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/browser/ui/ash/session/test_session_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash {

using holding_space::ScopedTestMountPoint;

namespace {

namespace mojo_ipc = scanning::mojom;

using ProtoScanFailureMode = lorgnette::ScanFailureMode;

// Path to the user's "My files" folder.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";

// Scanner names used for tests.
constexpr char kFirstTestScannerName[] = "Test Scanner 1";
constexpr char16_t kFirstTestScannerName16[] = u"Test Scanner 1";
constexpr char kSecondTestScannerName[] = "Test Scanner 2";
constexpr char16_t kSecondTestScannerName16[] = u"Test Scanner 2";
constexpr char kEpsonTestName[] =
    "airscan:escl:EPSON XP-7100 Series:http://100.107.108.190:443/eSCL/";

// Document source name used for tests.
constexpr char kDocumentSourceName[] = "Flatbed";
constexpr char kAdfSourceName[] = "ADF Duplex";

// Resolutions used for tests.
constexpr uint32_t kFirstResolution = 75;
constexpr uint32_t kSecondResolution = 300;

// Email used for test profile.
constexpr char kUserEmail[] = "user@email.com";

// Translation from file type to saved file extension.
const std::map<mojo_ipc::FileType, std::string> kFileTypes = {
    {mojo_ipc::FileType::kJpg, "jpg"},
    {mojo_ipc::FileType::kPdf, "pdf"},
    {mojo_ipc::FileType::kPng, "png"}};

// Returns a DocumentSource object.
lorgnette::DocumentSource CreateLorgnetteDocumentSource() {
  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SOURCE_PLATEN);
  source.set_name(kDocumentSourceName);
  source.add_color_modes(lorgnette::MODE_COLOR);
  source.add_resolutions(kFirstResolution);
  source.add_resolutions(kSecondResolution);
  return source;
}

// Returns an ADF Duplex DocumentSource object.
lorgnette::DocumentSource CreateAdfDuplexDocumentSource() {
  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SOURCE_ADF_DUPLEX);
  source.set_name(kAdfSourceName);
  return source;
}

// Returns a ScannerCapabilities object.
lorgnette::ScannerCapabilities CreateLorgnetteScannerCapabilities() {
  lorgnette::ScannerCapabilities caps;
  *caps.add_sources() = CreateLorgnetteDocumentSource();
  return caps;
}

// Returns a ScannerCapabilities object used for testing a scanner
// that flips alternate pages..
lorgnette::ScannerCapabilities CreateEpsonScannerCapabilities() {
  lorgnette::ScannerCapabilities caps;
  *caps.add_sources() = CreateAdfDuplexDocumentSource();
  caps.add_color_modes(lorgnette::MODE_COLOR);
  caps.add_resolutions(kFirstResolution);
  caps.add_resolutions(kSecondResolution);
  return caps;
}

std::string GetTimestamp(const base::Time& scan_time) {
  return base::UnlocalizedTimeFormatWithPattern(scan_time, "yyMMdd-HHmmss");
}

// Returns single FilePath to mimic saved PDF format scan.
base::FilePath CreateSavedPdfScanPath(const base::FilePath& dir,
                                      const base::Time& scan_time) {
  return dir.Append(
      base::StringPrintf("scan_%s.pdf", GetTimestamp(scan_time).c_str()));
}

// Returns a vector of FilePaths to mimic saved scans.
std::vector<base::FilePath> CreateSavedScanPaths(
    const base::FilePath& dir,
    const base::Time& scan_time,
    const mojo_ipc::FileType& file_type,
    int num_pages_to_scan) {
  const auto typeAndExtension = kFileTypes.find(file_type);
  EXPECT_NE(typeAndExtension, kFileTypes.cend());
  std::vector<base::FilePath> file_paths;
  if (file_type == mojo_ipc::FileType::kPdf) {
    file_paths.reserve(1);
    file_paths.push_back(CreateSavedPdfScanPath(dir, scan_time));
  } else {
    file_paths.reserve(num_pages_to_scan);
    for (int i = 1; i <= num_pages_to_scan; i++) {
      file_paths.push_back(dir.Append(
          base::StringPrintf("scan_%s_%d.%s", GetTimestamp(scan_time).c_str(),
                             i, typeAndExtension->second.c_str())));
    }
  }
  return file_paths;
}

// Returns a manually generated JPEG image. |alpha| is used to make them unique.
std::string CreateJpeg(const int alpha = 255) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseARGB(alpha, 0, 0, 255);
  std::vector<unsigned char> bytes;
  CHECK(gfx::JPEGCodec::Encode(bitmap, 90, &bytes));
  return std::string(bytes.begin(), bytes.end());
}

// Returns scan settings with the given path and file type.
mojo_ipc::ScanSettings CreateScanSettings(
    const base::FilePath& scan_to_path,
    const mojo_ipc::FileType& file_type,
    const std::string& source = "",
    mojo_ipc::ColorMode color_mode = mojo_ipc::ColorMode::kColor,
    mojo_ipc::PageSize page_size = mojo_ipc::PageSize::kIsoA3,
    uint32_t resolution = kFirstResolution) {
  mojo_ipc::ScanSettings settings;
  settings.scan_to_path = scan_to_path;
  settings.file_type = file_type;
  settings.source_name = source;
  settings.page_size = page_size;
  settings.color_mode = color_mode;
  settings.resolution_dpi = resolution;
  return settings;
}

// Returns a profile manager set up to generate testing profiles.
std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  auto profile_manager = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

}  // namespace

class FakeScanJobObserver : public mojo_ipc::ScanJobObserver {
 public:
  FakeScanJobObserver() = default;
  ~FakeScanJobObserver() override = default;

  FakeScanJobObserver(const FakeScanJobObserver&) = delete;
  FakeScanJobObserver& operator=(const FakeScanJobObserver&) = delete;

  // mojo_ipc::ScanJobObserver:
  void OnPageProgress(uint32_t page_number,
                      uint32_t progress_percent) override {
    progress_ = progress_percent;
  }

  void OnPageComplete(const std::vector<uint8_t>& page_data,
                      const uint32_t new_page_index) override {
    page_complete_ = true;
    new_page_index_ = new_page_index;
  }

  void OnScanComplete(
      ProtoScanFailureMode result,
      const std::vector<base::FilePath>& scanned_file_paths) override {
    scan_result_ = result;
    scanned_file_paths_ = scanned_file_paths;
  }

  void OnCancelComplete(bool success) override {
    cancel_scan_success_ = success;
  }

  void OnMultiPageScanFail(ProtoScanFailureMode result) override {
    multi_page_scan_result_ = result;
  }

  // Creates a pending remote that can be passed in calls to
  // ScanService::StartScan().
  mojo::PendingRemote<mojo_ipc::ScanJobObserver> GenerateRemote() {
    if (receiver_.is_bound())
      receiver_.reset();

    mojo::PendingRemote<mojo_ipc::ScanJobObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // Returns true if the scan completed successfully.
  bool scan_success() const {
    return progress_ == 100 && page_complete_ &&
           scan_result_ == ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE;
  }

  // Returns true if the cancel scan request completed successfully.
  bool cancel_scan_success() const { return cancel_scan_success_; }

  uint32_t new_page_index() const { return new_page_index_; }

  // Returns the result of the scan job.
  ProtoScanFailureMode scan_result() const { return scan_result_; }

  // Returns the result of the multi-page scan job.
  ProtoScanFailureMode multi_page_scan_result() const {
    return multi_page_scan_result_;
  }

  // Returns file paths of the saved scan files.
  std::vector<base::FilePath> scanned_file_paths() const {
    return scanned_file_paths_;
  }

 private:
  uint32_t progress_ = 0;
  bool page_complete_ = false;
  uint32_t new_page_index_ = UINT32_MAX;
  ProtoScanFailureMode scan_result_ =
      ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN;
  ProtoScanFailureMode multi_page_scan_result_ =
      ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN;
  bool cancel_scan_success_ = false;
  std::vector<base::FilePath> scanned_file_paths_;
  mojo::Receiver<mojo_ipc::ScanJobObserver> receiver_{this};
};

class ScanServiceTest : public testing::Test {
 public:
  ScanServiceTest()
      : profile_manager_(CreateTestingProfileManager()),
        profile_(profile_manager_->CreateTestingProfile(kUserEmail)),
        scanned_files_mount_(
            ScopedTestMountPoint::CreateAndMountDownloads(profile_)),
        session_controller_(std::make_unique<TestSessionController>()),
        user_manager_(new ash::FakeChromeUserManager),
        user_manager_owner_(base::WrapUnique(user_manager_.get())) {
    DCHECK(scanned_files_mount_->IsValid());
    const AccountId account_id(AccountId::FromUserEmail(kUserEmail));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    SetupScanService(scanned_files_mount_->GetRootPath(),
                     base::FilePath("/google/drive"));
  }

  void SetupScanService(base::FilePath my_files_path,
                        base::FilePath google_drive_path) {
    scan_service_ = std::make_unique<ScanService>(
        &fake_lorgnette_scanner_manager_, my_files_path, google_drive_path,
        profile_);
    if (scan_service_remote_.is_bound()) {
      scan_service_remote_.reset();
    }
    scan_service_->BindInterface(
        scan_service_remote_.BindNewPipeAndPassReceiver());
  }

  // Gets scanners by calling ScanService::GetScanners() via the mojo::Remote.
  std::vector<mojo_ipc::ScannerPtr> GetScanners() {
    base::test::TestFuture<std::vector<mojo_ipc::ScannerPtr>> future;
    scan_service_remote_->GetScanners(future.GetCallback());
    return future.Take();
  }

  // Gets scanner capabilities for the scanner identified by |scanner_id| by
  // calling ScanService::GetScannerCapabilities() via the mojo::Remote.
  mojo_ipc::ScannerCapabilitiesPtr GetScannerCapabilities(
      const base::UnguessableToken& scanner_id) {
    base::test::TestFuture<mojo_ipc::ScannerCapabilitiesPtr> future;
    scan_service_remote_->GetScannerCapabilities(scanner_id,
                                                 future.GetCallback());
    return future.Take();
  }

  // Starts a scan with the scanner identified by |scanner_id| with the given
  // |settings| by calling ScanService::StartScan() via the mojo::Remote.
  bool StartScan(const base::UnguessableToken& scanner_id,
                 mojo_ipc::ScanSettingsPtr settings) {
    base::test::TestFuture<bool> future;
    scan_service_remote_->StartScan(scanner_id, std::move(settings),
                                    fake_scan_job_observer_.GenerateRemote(),
                                    future.GetCallback());
    bool success = future.Take();
    task_environment_.RunUntilIdle();
    return success;
  }

  // Starts a multi-page scan with the scanner identified by |scanner_id| with
  // the given |settings| by calling ScanService::StartMultiPageScan() via the
  // mojo::Remote. Binds the returned MultiPageScanController
  // mojo::PendingRemote.
  bool StartMultiPageScan(const base::UnguessableToken& scanner_id,
                          mojo_ipc::ScanSettingsPtr settings) {
    base::test::TestFuture<
        mojo::PendingRemote<mojo_ipc::MultiPageScanController>>
        future;
    scan_service_remote_->StartMultiPageScan(
        scanner_id, std::move(settings),
        fake_scan_job_observer_.GenerateRemote(), future.GetCallback());
    auto pending_remote = future.Take();
    if (!pending_remote.is_valid())
      return false;

    multi_page_scan_controller_remote_.Bind(std::move(pending_remote));
    task_environment_.RunUntilIdle();
    return true;
  }

  void ResetMultiPageScanControllerRemote() {
    multi_page_scan_controller_remote_.reset();
  }

  bool ScanNextPage(const base::UnguessableToken& scanner_id,
                    mojo_ipc::ScanSettingsPtr settings) {
    base::test::TestFuture<bool> future;
    multi_page_scan_controller_remote_->ScanNextPage(
        scanner_id, std::move(settings), future.GetCallback());
    bool success = future.Take();
    task_environment_.RunUntilIdle();
    return success;
  }

  // Performs a cancel scan request.
  void CancelScan() {
    scan_service_remote_->CancelScan();
    task_environment_.RunUntilIdle();
  }

  void CompleteMultiPageScan() {
    multi_page_scan_controller_remote_->CompleteMultiPageScan();
    task_environment_.RunUntilIdle();
  }

  void RemovePage(const uint32_t page_index) {
    multi_page_scan_controller_remote_->RemovePage(page_index);
    task_environment_.RunUntilIdle();
  }

  bool RescanPage(const base::UnguessableToken& scanner_id,
                  mojo_ipc::ScanSettingsPtr settings,
                  const uint32_t page_index) {
    base::test::TestFuture<bool> future;
    multi_page_scan_controller_remote_->RescanPage(
        scanner_id, std::move(settings), page_index, future.GetCallback());
    bool success = future.Take();
    task_environment_.RunUntilIdle();
    return success;
  }

 protected:
  // A `BrowserTaskEnvironment` allows the test to create a `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeLorgnetteScannerManager fake_lorgnette_scanner_manager_;
  FakeScanJobObserver fake_scan_job_observer_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  const raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ScopedTestMountPoint> scanned_files_mount_;
  std::unique_ptr<TestSessionController> session_controller_;
  const raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager user_manager_owner_;
  std::unique_ptr<ScanService> scan_service_;

 private:
  mojo::Remote<mojo_ipc::ScanService> scan_service_remote_;
  mojo::Remote<mojo_ipc::MultiPageScanController>
      multi_page_scan_controller_remote_;
};

// Test that no scanners are returned when there are no scanner names.
TEST_F(ScanServiceTest, NoScannerNames) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse({});
  auto scanners = GetScanners();
  EXPECT_TRUE(scanners.empty());
}

// Test that a scanner is returned with the correct display name.
TEST_F(ScanServiceTest, GetScanners) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);
  EXPECT_EQ(scanners[0]->display_name, kFirstTestScannerName16);
}

// Test that two returned scanners have unique IDs.
TEST_F(ScanServiceTest, UniqueScannerIds) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName, kSecondTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 2u);
  EXPECT_EQ(scanners[0]->display_name, kFirstTestScannerName16);
  EXPECT_EQ(scanners[1]->display_name, kSecondTestScannerName16);
  EXPECT_NE(scanners[0]->id, scanners[1]->id);
}

// Test that the number of detected scanners is recorded.
TEST_F(ScanServiceTest, RecordNumDetectedScanners) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Scanning.NumDetectedScanners", 0);
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName, kSecondTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 2u);
  histogram_tester.ExpectUniqueSample("Scanning.NumDetectedScanners", 2, 1);
}

// Test that attempting to get capabilities with a scanner ID that doesn't
// correspond to a scanner results in obtaining no capabilities.
TEST_F(ScanServiceTest, BadScannerId) {
  auto caps = GetScannerCapabilities(base::UnguessableToken::Create());
  EXPECT_TRUE(caps->sources.empty());
}

// Test that failing to obtain capabilities from the LorgnetteScannerManager
// results in obtaining no capabilities.
TEST_F(ScanServiceTest, NoCapabilities) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  fake_lorgnette_scanner_manager_.SetGetScannerCapabilitiesResponse(
      std::nullopt);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);
  auto caps = GetScannerCapabilities(scanners[0]->id);
  EXPECT_TRUE(caps->sources.empty());
}

// Test that scanner capabilities can be obtained successfully.
TEST_F(ScanServiceTest, GetScannerCapabilities) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  fake_lorgnette_scanner_manager_.SetGetScannerCapabilitiesResponse(
      CreateLorgnetteScannerCapabilities());
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);
  auto caps = GetScannerCapabilities(scanners[0]->id);
  ASSERT_EQ(caps->sources.size(), 1u);
  EXPECT_EQ(caps->sources[0]->type, mojo_ipc::SourceType::kFlatbed);
  EXPECT_EQ(caps->sources[0]->name, kDocumentSourceName);
  ASSERT_EQ(caps->sources[0]->color_modes.size(), 1u);
  EXPECT_EQ(caps->sources[0]->color_modes[0], mojo_ipc::ColorMode::kColor);
  ASSERT_EQ(caps->sources[0]->resolutions.size(), 2u);
  EXPECT_EQ(caps->sources[0]->resolutions[0], kFirstResolution);
  EXPECT_EQ(caps->sources[0]->resolutions[1], kSecondResolution);
}

// Test that attempting to scan with a scanner ID that doesn't correspond to a
// scanner results in a failed scan.
TEST_F(ScanServiceTest, ScanWithBadScannerId) {
  EXPECT_FALSE(StartScan(base::UnguessableToken::Create(),
                         mojo_ipc::ScanSettings::New()));
}

// Test that attempting to scan with an unsupported file path fails.
// Specifically, use a file path with directory navigation (e.g. "..") to verify
// it can't be used to save scanned images to an unsupported path.
TEST_F(ScanServiceTest, ScanWithUnsupportedFilePath) {
  const base::FilePath my_files_path(kMyFilesPath);
  SetupScanService(my_files_path, base::FilePath("/google/drive"));

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {"TestData"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const mojo_ipc::ScanSettings settings = CreateScanSettings(
      my_files_path.Append("../../../var/log"), mojo_ipc::FileType::kPng);
  EXPECT_FALSE(StartScan(scanners[0]->id, settings.Clone()));
}

// Test that a scan can be performed successfully.
TEST_F(ScanServiceTest, Scan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreateJpeg(), CreateJpeg(),
                                              CreateJpeg()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const auto now = base::Time::Now();
  base::HistogramTester histogram_tester;
  int num_single_file_scans = 0u;
  int num_multi_file_scans = 0u;
  for (int type_num = static_cast<int>(mojo_ipc::FileType::kMinValue);
       type_num <= static_cast<int>(mojo_ipc::FileType::kMaxValue);
       ++type_num) {
    auto type = static_cast<mojo_ipc::FileType>(type_num);

    const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
        scanned_files_mount_->GetRootPath(), now, type, scan_data.size());
    for (const auto& saved_scan_path : saved_scan_paths)
      EXPECT_FALSE(base::PathExists(saved_scan_path));

    mojo_ipc::ScanSettings settings =
        CreateScanSettings(scanned_files_mount_->GetRootPath(), type);
    EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
    for (const auto& saved_scan_path : saved_scan_paths)
      EXPECT_TRUE(base::PathExists(saved_scan_path));

    EXPECT_TRUE(fake_scan_job_observer_.scan_success());
    EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE,
              fake_scan_job_observer_.scan_result());
    EXPECT_EQ(scan_data.size() - 1, fake_scan_job_observer_.new_page_index());
    EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());

    // Verify that the histograms have been updated correctly.
    histogram_tester.ExpectBucketCount(
        "Scanning.NumFilesCreated", saved_scan_paths.size(),
        type == mojo_ipc::FileType::kPdf ? ++num_single_file_scans
                                         : ++num_multi_file_scans);
    histogram_tester.ExpectUniqueSample(
        "Scanning.NumPagesScanned", scan_data.size(),
        num_single_file_scans + num_multi_file_scans);
  }
}

// Test that an Epson ADF Duplex scan, which produces flipped pages, completes
// successfully.
TEST_F(ScanServiceTest, RotateEpsonADF) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse({kEpsonTestName});
  fake_lorgnette_scanner_manager_.SetGetScannerCapabilitiesResponse(
      CreateEpsonScannerCapabilities());
  const std::vector<std::string> scan_data = {CreateJpeg(), CreateJpeg(),
                                              CreateJpeg()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings =
      CreateScanSettings(scanned_files_mount_->GetRootPath(),
                         mojo_ipc::FileType::kPdf, "ADF Duplex");
  const base::FilePath saved_scan_path = CreateSavedPdfScanPath(
      scanned_files_mount_->GetRootPath(), base::Time::Now());
  EXPECT_FALSE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_TRUE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  const std::vector<base::FilePath> scanned_file_paths =
      fake_scan_job_observer_.scanned_file_paths();
  EXPECT_EQ(1u, scanned_file_paths.size());
  EXPECT_EQ(scan_data.size() - 1, fake_scan_job_observer_.new_page_index());
  EXPECT_EQ(saved_scan_path, scanned_file_paths.front());
}

// Test that when a scan fails, the scan job is marked as failed.
TEST_F(ScanServiceTest, ScanFails) {
  // Skip setting the scan data in FakeLorgnetteScannerManager so the scan will
  // fail.
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPng);

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY,
            fake_scan_job_observer_.scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
}

// Tests that a new scan job can succeed after the previous scan failed.
TEST_F(ScanServiceTest, ScanAfterFailedScan) {
  // Skip setting the scan data in FakeLorgnetteScannerManager so the scan will
  // fail.
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPng);

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY,
            fake_scan_job_observer_.scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());

  // Set scan data so next scan is successful.
  const std::vector<std::string> scan_data = {"TestData1", "TestData2",
                                              "TestData3"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);

  const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
      scanned_files_mount_->GetRootPath(), base::Time::Now(),
      mojo_ipc::FileType::kPng, scan_data.size());
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_TRUE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE,
            fake_scan_job_observer_.scan_result());
  EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());
  EXPECT_EQ(scan_data.size() - 1, fake_scan_job_observer_.new_page_index());
}

// Tests that a failed scan does not retain values from the previous successful
// scan.
TEST_F(ScanServiceTest, FailedScanAfterSuccessfulScan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {"TestData1", "TestData2",
                                              "TestData3"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPng);
  const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
      scanned_files_mount_->GetRootPath(), base::Time::Now(),
      mojo_ipc::FileType::kPng, scan_data.size());
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_TRUE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE,
            fake_scan_job_observer_.scan_result());
  EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());
  EXPECT_EQ(scan_data.size() - 1, fake_scan_job_observer_.new_page_index());

  // Remove the scan data from FakeLorgnetteScannerManager so the scan will
  // fail.
  fake_lorgnette_scanner_manager_.SetScanResponse({});

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY,
            fake_scan_job_observer_.scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
}

// Test that canceling sends an update to the observer OnCancelComplete().
TEST_F(ScanServiceTest, CancelScanBeforeScanCompletes) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {"TestData"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPng);

  StartScan(scanners[0]->id, settings.Clone());
  CancelScan();
  EXPECT_TRUE(fake_scan_job_observer_.cancel_scan_success());
}

// Test that a multi-page image scan creates a holding space item.
TEST_F(ScanServiceTest, HoldingSpaceScan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreateJpeg(), CreateJpeg(),
                                              CreateJpeg()};
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  // Verify that the holding space starts out empty.
  HoldingSpaceKeyedService* holding_space_keyed_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile_);
  ASSERT_TRUE(holding_space_keyed_service);
  const HoldingSpaceModel* holding_space_model =
      holding_space_keyed_service->model_for_testing();
  ASSERT_TRUE(holding_space_model);
  size_t num_items_in_holding_space = 0u;
  ASSERT_EQ(num_items_in_holding_space, holding_space_model->items().size());

  const auto now = base::Time::Now();
  for (int type_num = static_cast<int>(mojo_ipc::FileType::kMinValue);
       type_num <= static_cast<int>(mojo_ipc::FileType::kMaxValue);
       ++type_num) {
    auto type = static_cast<mojo_ipc::FileType>(type_num);

    fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
    const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
        scanned_files_mount_->GetRootPath(), now, type, scan_data.size());
    for (const auto& saved_scan_path : saved_scan_paths)
      EXPECT_FALSE(base::PathExists(saved_scan_path));

    mojo_ipc::ScanSettings settings =
        CreateScanSettings(scanned_files_mount_->GetRootPath(), type);
    EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
    EXPECT_TRUE(fake_scan_job_observer_.scan_success());
    EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE,
              fake_scan_job_observer_.scan_result());
    EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());

    // Verify that the all pages of each scan are added to the holding space.
    EXPECT_EQ(num_items_in_holding_space + saved_scan_paths.size(),
              holding_space_model->items().size());
    for (const auto& saved_scan_path : saved_scan_paths) {
      EXPECT_TRUE(base::PathExists(saved_scan_path));
      HoldingSpaceItem* scanned_item =
          holding_space_model->items()[num_items_in_holding_space++].get();
      EXPECT_EQ(scanned_item->type(), HoldingSpaceItem::Type::kScan);
      EXPECT_EQ(scanned_item->file().file_path, saved_scan_path);
    }

    // Remove the scan data from FakeLorgnetteScannerManager so the scan will
    // fail.
    fake_lorgnette_scanner_manager_.SetScanResponse({});

    EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
    EXPECT_FALSE(fake_scan_job_observer_.scan_success());
    EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY,
              fake_scan_job_observer_.scan_result());
    EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());

    // Verify that no item is added to the holding space when a scan fails.
    EXPECT_EQ(num_items_in_holding_space, holding_space_model->items().size());
  }
}

// Test that a multi-page scan can be performed successfully.
TEST_F(ScanServiceTest, MultiPageScan) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreateJpeg()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
      scanned_files_mount_->GetRootPath(), base::Time::Now(),
      mojo_ipc::FileType::kPdf, scan_data.size());
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);
  uint32_t new_page_index = 0;

  // Scan the first page without completing the scan.
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Scan the second page without completing the scan.
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Complete the multi-page scan expecting 2 pages to be scanned and a single
  // PDF to be created.
  CompleteMultiPageScan();
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_TRUE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());
  histogram_tester.ExpectUniqueSample("Scanning.NumPagesScanned", 2, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.NumPagesScanned",
                                      2, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.PageScanResult",
                                      scanning::ScanJobFailureReason::kSuccess,
                                      2);
}

// Test that when a multi-page scan fails, the scan job is marked as failed.
TEST_F(ScanServiceTest, MultiPageScanFails) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreateJpeg()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);

  // The first scan should pass with no failure.
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN,
            fake_scan_job_observer_.multi_page_scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
  EXPECT_EQ(0u, fake_scan_job_observer_.new_page_index());

  // Set scan data to empty vector in FakeLorgnetteScannerManager so the next
  // scan will fail.
  fake_lorgnette_scanner_manager_.SetScanResponse({});
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY,
            fake_scan_job_observer_.multi_page_scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());

  histogram_tester.ExpectBucketCount("Scanning.MultiPageScan.PageScanResult",
                                     scanning::ScanJobFailureReason::kSuccess,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Scanning.MultiPageScan.PageScanResult",
      scanning::ScanJobFailureReason::kDeviceBusy, 1);
}

// Test that attempting to start a second multi-page scan while another
// multi-page scan session is going will fail.
TEST_F(ScanServiceTest, StartingAnotherMultiPageScan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreateJpeg()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);

  // The first scan should pass with no failure.
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN,
            fake_scan_job_observer_.multi_page_scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
  EXPECT_EQ(0u, fake_scan_job_observer_.new_page_index());

  // The second attempt should fail.
  EXPECT_FALSE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
}

// Test that a page can be removed from a multi-page scan with two scanned
// images.
TEST_F(ScanServiceTest, MultiPageScanRemoveWithTwoPages) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);
  uint32_t new_page_index = 0;

  const std::string first_scanned_image = CreateJpeg(/*alpha=*/1);
  const std::vector<std::string> first_scan_data = {first_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(first_scan_data);
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  const std::string second_scanned_image = CreateJpeg(/*alpha=*/2);
  const std::vector<std::string> second_scan_data = {second_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(second_scan_data);
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Delete the first page.
  RemovePage(0);
  CompleteMultiPageScan();

  const std::vector<std::string> scanned_images =
      scan_service_->GetScannedImagesForTesting();
  EXPECT_EQ(1u, scanned_images.size());
  EXPECT_EQ(second_scanned_image, scanned_images[0]);

  // Expect 1 record of the Scanning.NumPagesScanned metric in the 1 pages
  // scanned bucket.
  histogram_tester.ExpectUniqueSample("Scanning.NumPagesScanned", 1, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.NumPagesScanned",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRemovePage, 1);
}

// Test that a page can be removed from a multi-page scan with three scanned
// images.
TEST_F(ScanServiceTest, MultiPageScanRemoveWithThreePages) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);
  uint32_t new_page_index = 0;

  const std::string first_scanned_image = CreateJpeg(/*alpha=*/1);
  const std::vector<std::string> first_scan_data = {first_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(first_scan_data);
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  const std::string second_scanned_image = CreateJpeg(/*alpha=*/2);
  const std::vector<std::string> second_scan_data = {second_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(second_scan_data);
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  const std::string third_scanned_image = CreateJpeg(/*alpha=*/3);
  const std::vector<std::string> third_scan_data = {third_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(third_scan_data);
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Delete the second page.
  RemovePage(1);
  CompleteMultiPageScan();

  const std::vector<std::string> scanned_images =
      scan_service_->GetScannedImagesForTesting();
  EXPECT_EQ(2u, scanned_images.size());
  EXPECT_EQ(first_scanned_image, scanned_images[0]);
  EXPECT_EQ(third_scanned_image, scanned_images[1]);

  // Expect 1 record of the Scanning.NumPagesScanned metric in the 2 pages
  // scanned bucket.
  histogram_tester.ExpectUniqueSample("Scanning.NumPagesScanned", 2, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.NumPagesScanned",
                                      2, 1);
  histogram_tester.ExpectUniqueSample(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRemovePage, 1);
}

// Test that if there's only one page available, the page is removed and the
// multi-page scan session is reset and a new session can be started.
TEST_F(ScanServiceTest, MultiPageScanRemoveLastPage) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreateJpeg()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);
  uint32_t new_page_index = 0;

  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Removing the page should reset the multi-page scan session.
  RemovePage(0);
  --new_page_index;

  // Start a new scan and complete it with 1 page.
  ResetMultiPageScanControllerRemote();
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());
  CompleteMultiPageScan();

  const std::vector<std::string> scanned_images =
      scan_service_->GetScannedImagesForTesting();
  EXPECT_EQ(1u, scanned_images.size());

  // Expect 1 record of the Scanning.NumPagesScanned metric in the 1 page
  // scanned bucket.
  histogram_tester.ExpectUniqueSample("Scanning.NumPagesScanned", 1, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.NumPagesScanned",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRemovePage, 1);
}

// Test that a page can be rescanned and replaced from a multi-page scan with
// one scanned image.
TEST_F(ScanServiceTest, MultiPageScanRescanWithOnePage) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);
  uint32_t new_page_index = 0;

  const std::string first_scanned_image = CreateJpeg(/*alpha=*/1);
  const std::vector<std::string> first_scan_data = {first_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(first_scan_data);
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Rescan the page.
  const std::string rescanned_scanned_image = CreateJpeg(/*alpha=*/2);
  const std::vector<std::string> rescanned_scan_data = {
      rescanned_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(rescanned_scan_data);
  EXPECT_TRUE(RescanPage(scanners[0]->id, settings.Clone(), /*page_index=*/0));
  EXPECT_EQ(0u, fake_scan_job_observer_.new_page_index());
  CompleteMultiPageScan();

  const std::vector<std::string> scanned_images =
      scan_service_->GetScannedImagesForTesting();
  EXPECT_EQ(1u, scanned_images.size());
  EXPECT_EQ(rescanned_scanned_image, scanned_images[0]);

  // Expect 1 record of the Scanning.NumPagesScanned metric in the 1 pages
  // scanned bucket.
  histogram_tester.ExpectUniqueSample("Scanning.NumPagesScanned", 1, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.NumPagesScanned",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRescanPage, 1);
}

// Test that a page can be rescanned and replaced from a multi-page scan with
// three scanned images.
TEST_F(ScanServiceTest, MultiPageScanRescanWithThreePages) {
  base::HistogramTester histogram_tester;

  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf);
  uint32_t new_page_index = 0;

  const std::string first_scanned_image = CreateJpeg(/*alpha=*/1);
  const std::vector<std::string> first_scan_data = {first_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(first_scan_data);
  EXPECT_TRUE(StartMultiPageScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  const std::string second_scanned_image = CreateJpeg(/*alpha=*/2);
  const std::vector<std::string> second_scan_data = {second_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(second_scan_data);
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  const std::string third_scanned_image = CreateJpeg(/*alpha=*/3);
  const std::vector<std::string> third_scan_data = {third_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(third_scan_data);
  EXPECT_TRUE(ScanNextPage(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(new_page_index++, fake_scan_job_observer_.new_page_index());

  // Rescan the second page.
  const std::string rescanned_scanned_image = CreateJpeg(/*alpha=*/4);
  const std::vector<std::string> rescanned_scan_data = {
      rescanned_scanned_image};
  fake_lorgnette_scanner_manager_.SetScanResponse(rescanned_scan_data);
  EXPECT_TRUE(RescanPage(scanners[0]->id, settings.Clone(), /*page_index=*/1));
  EXPECT_EQ(1u, fake_scan_job_observer_.new_page_index());
  CompleteMultiPageScan();

  const std::vector<std::string> scanned_images =
      scan_service_->GetScannedImagesForTesting();
  EXPECT_EQ(3u, scanned_images.size());
  EXPECT_EQ(first_scanned_image, scanned_images[0]);
  EXPECT_EQ(rescanned_scanned_image, scanned_images[1]);
  EXPECT_EQ(third_scanned_image, scanned_images[2]);

  // Expect 1 record of the Scanning.NumPagesScanned metric in the 3 pages
  // scanned bucket.
  histogram_tester.ExpectUniqueSample("Scanning.NumPagesScanned", 3, 1);
  histogram_tester.ExpectUniqueSample("Scanning.MultiPageScan.NumPagesScanned",
                                      3, 1);
  histogram_tester.ExpectUniqueSample(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRescanPage, 1);
}

TEST_F(ScanServiceTest, ResetReceiverOnBindInterface) {
  // This test simulates a user refreshing the WebUI page. The receiver should
  // be reset before binding the new receiver. Otherwise we would get a DCHECK
  // error from mojo::Receiver
  mojo::Remote<scanning::mojom::ScanService> remote;
  scan_service_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  remote.reset();

  scan_service_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

// TODO(b:307385730): Parameterize this test once more settings combinations
// are added.
TEST_F(ScanServiceTest, ScanDataSettings) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  // Settings correspond to "flatbed_jpeg_color_letter_300_dpi"
  // which sets the `alpha` used in the generated JPEG images to
  // 1.
  mojo_ipc::ScanSettings settings = CreateScanSettings(
      scanned_files_mount_->GetRootPath(), mojo_ipc::FileType::kPdf, "flatbed",
      mojo_ipc::ColorMode::kColor, mojo_ipc::PageSize::kNaLetter,
      kSecondResolution);

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_EQ(1u, scan_service_->GetScannedImagesForTesting().size());
  EXPECT_EQ(CreateJpeg(/*alpha=*/1),
            scan_service_->GetScannedImagesForTesting()[0]);
}

}  // namespace ash
