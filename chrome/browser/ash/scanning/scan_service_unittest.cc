// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_service.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ash/content/scanning/mojom/scanning.mojom-test-utils.h"
#include "ash/content/scanning/mojom/scanning.mojom.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace ash {

namespace {

namespace mojo_ipc = scanning::mojom;

// Path to the user's "My files" folder.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";

// Scanner names used for tests.
constexpr char kFirstTestScannerName[] = "Test Scanner 1";
constexpr char kSecondTestScannerName[] = "Test Scanner 2";
constexpr char kEpsonTestName[] = "Epson";

// Document source name used for tests.
constexpr char kDocumentSourceName[] = "Flatbed";
constexpr char kAdfSourceName[] = "ADF Duplex";

// Resolutions used for tests.
constexpr uint32_t kFirstResolution = 75;
constexpr uint32_t kSecondResolution = 300;

// Returns a DocumentSource object.
lorgnette::DocumentSource CreateLorgnetteDocumentSource() {
  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SOURCE_PLATEN);
  source.set_name(kDocumentSourceName);
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
  caps.add_color_modes(lorgnette::MODE_COLOR);
  caps.add_resolutions(kFirstResolution);
  caps.add_resolutions(kSecondResolution);
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

// Returns a vector of FilePaths to mimic saved scans.
std::vector<base::FilePath> CreateSavedScanPaths(
    const base::FilePath& dir,
    const base::Time::Exploded& scan_time,
    const std::string& type,
    int num_pages_to_scan) {
  std::vector<base::FilePath> file_paths;
  file_paths.reserve(num_pages_to_scan);
  for (int i = 1; i <= num_pages_to_scan; i++) {
    file_paths.push_back(dir.Append(base::StringPrintf(
        "scan_%02d%02d%02d-%02d%02d%02d_%d.%s", scan_time.year, scan_time.month,
        scan_time.day_of_month, scan_time.hour, scan_time.minute,
        scan_time.second, i, type.c_str())));
  }
  return file_paths;
}

// Returns single FilePath to mimic saved PDF format scan.
base::FilePath CreateSavedPdfScanPath(const base::FilePath& dir,
                                      const base::Time::Exploded& scan_time) {
  return dir.Append(base::StringPrintf("scan_%02d%02d%02d-%02d%02d%02d.pdf",
                                       scan_time.year, scan_time.month,
                                       scan_time.day_of_month, scan_time.hour,
                                       scan_time.minute, scan_time.second));
}

// Returns a manually generated PNG image.
std::string CreatePng() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseARGB(255, 0, 255, 0);
  std::vector<unsigned char> bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bytes);
  return std::string(bytes.begin(), bytes.end());
}

// Returns scan settings with the given path and file type.
mojo_ipc::ScanSettings CreateScanSettings(const base::FilePath& scan_to_path,
                                          const mojo_ipc::FileType& file_type) {
  mojo_ipc::ScanSettings settings;
  settings.scan_to_path = scan_to_path;
  settings.file_type = file_type;
  return settings;
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

  void OnPageComplete(const std::vector<uint8_t>& page_data) override {
    page_complete_ = true;
  }

  void OnScanComplete(
      mojo_ipc::ScanResult result,
      const std::vector<base::FilePath>& scanned_file_paths) override {
    scan_result_ = result;
    scanned_file_paths_ = scanned_file_paths;
  }

  void OnCancelComplete(bool success) override {
    cancel_scan_success_ = success;
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
           scan_result_ == mojo_ipc::ScanResult::kSuccess;
  }

  // Returns true if the cancel scan request completed successfully.
  bool cancel_scan_success() const { return cancel_scan_success_; }

  // Returns the result of the scan job.
  mojo_ipc::ScanResult scan_result() const { return scan_result_; }

  // Returns file paths of the saved scan files.
  std::vector<base::FilePath> scanned_file_paths() const {
    return scanned_file_paths_;
  }

 private:
  uint32_t progress_ = 0;
  bool page_complete_ = false;
  mojo_ipc::ScanResult scan_result_ = mojo_ipc::ScanResult::kUnknownError;
  bool cancel_scan_success_ = false;
  std::vector<base::FilePath> scanned_file_paths_;
  mojo::Receiver<mojo_ipc::ScanJobObserver> receiver_{this};
};

class ScanServiceTest : public testing::Test {
 public:
  ScanServiceTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scan_service_.BindInterface(
        scan_service_remote_.BindNewPipeAndPassReceiver());
  }

  // Gets scanners by calling ScanService::GetScanners() via the mojo::Remote.
  std::vector<mojo_ipc::ScannerPtr> GetScanners() {
    std::vector<mojo_ipc::ScannerPtr> scanners;
    mojo_ipc::ScanServiceAsyncWaiter(scan_service_remote_.get())
        .GetScanners(&scanners);
    return scanners;
  }

  // Gets scanner capabilities for the scanner identified by |scanner_id| by
  // calling ScanService::GetScannerCapabilities() via the mojo::Remote.
  mojo_ipc::ScannerCapabilitiesPtr GetScannerCapabilities(
      const base::UnguessableToken& scanner_id) {
    mojo_ipc::ScannerCapabilitiesPtr caps =
        mojo_ipc::ScannerCapabilities::New();
    mojo_ipc::ScanServiceAsyncWaiter(scan_service_remote_.get())
        .GetScannerCapabilities(scanner_id, &caps);
    return caps;
  }

  // Starts a scan with the scanner identified by |scanner_id| with the given
  // |settings| by calling ScanService::StartScan() via the mojo::Remote.
  bool StartScan(const base::UnguessableToken& scanner_id,
                 mojo_ipc::ScanSettingsPtr settings) {
    bool success;
    mojo_ipc::ScanServiceAsyncWaiter(scan_service_remote_.get())
        .StartScan(scanner_id, std::move(settings),
                   fake_scan_job_observer_.GenerateRemote(), &success);
    task_environment_.RunUntilIdle();
    return success;
  }

  // Performs a cancel scan request.
  void CancelScan() {
    scan_service_remote_->CancelScan();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  FakeLorgnetteScannerManager fake_lorgnette_scanner_manager_;
  FakeScanJobObserver fake_scan_job_observer_;
  ScanService scan_service_{&fake_lorgnette_scanner_manager_, base::FilePath(),
                            base::FilePath()};

 private:
  mojo::Remote<mojo_ipc::ScanService> scan_service_remote_;
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
  EXPECT_EQ(scanners[0]->display_name,
            base::UTF8ToUTF16(kFirstTestScannerName));
}

// Test that two returned scanners have unique IDs.
TEST_F(ScanServiceTest, UniqueScannerIds) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName, kSecondTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 2u);
  EXPECT_EQ(scanners[0]->display_name,
            base::UTF8ToUTF16(kFirstTestScannerName));
  EXPECT_EQ(scanners[1]->display_name,
            base::UTF8ToUTF16(kSecondTestScannerName));
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
  EXPECT_TRUE(caps->color_modes.empty());
  EXPECT_TRUE(caps->resolutions.empty());
}

// Test that failing to obtain capabilities from the LorgnetteScannerManager
// results in obtaining no capabilities.
TEST_F(ScanServiceTest, NoCapabilities) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  fake_lorgnette_scanner_manager_.SetGetScannerCapabilitiesResponse(
      base::nullopt);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);
  auto caps = GetScannerCapabilities(scanners[0]->id);
  EXPECT_TRUE(caps->sources.empty());
  EXPECT_TRUE(caps->color_modes.empty());
  EXPECT_TRUE(caps->resolutions.empty());
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
  ASSERT_EQ(caps->color_modes.size(), 1u);
  EXPECT_EQ(caps->color_modes[0], mojo_ipc::ColorMode::kColor);
  ASSERT_EQ(caps->resolutions.size(), 2u);
  EXPECT_EQ(caps->resolutions[0], kFirstResolution);
  EXPECT_EQ(caps->resolutions[1], kSecondResolution);
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
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {"TestData"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const base::FilePath my_files_path(kMyFilesPath);
  scan_service_.SetMyFilesPathForTesting(my_files_path);
  const mojo_ipc::ScanSettings settings = CreateScanSettings(
      my_files_path.Append("../../../var/log"), mojo_ipc::FileType::kPng);
  EXPECT_FALSE(StartScan(scanners[0]->id, settings.Clone()));
}

// Test that a scan can be performed successfully.
TEST_F(ScanServiceTest, Scan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreatePng(), CreatePng(),
                                              CreatePng()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  base::Time::Exploded scan_time;
  // Since we're using mock time, this is deterministic.
  base::Time::Now().LocalExplode(&scan_time);

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  std::map<std::string, mojo_ipc::FileType> file_types = {
      {"png", mojo_ipc::FileType::kPng}, {"jpg", mojo_ipc::FileType::kJpg}};
  for (const auto& type : file_types) {
    const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
        temp_dir_.GetPath(), scan_time, type.first, scan_data.size());
    for (const auto& saved_scan_path : saved_scan_paths)
      EXPECT_FALSE(base::PathExists(saved_scan_path));

    mojo_ipc::ScanSettings settings =
        CreateScanSettings(temp_dir_.GetPath(), type.second);
    EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
    for (const auto& saved_scan_path : saved_scan_paths)
      EXPECT_TRUE(base::PathExists(saved_scan_path));

    EXPECT_TRUE(fake_scan_job_observer_.scan_success());
    EXPECT_EQ(mojo_ipc::ScanResult::kSuccess,
              fake_scan_job_observer_.scan_result());
    EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());
  }
}

// Test that a scan with PDF file format can be perfomed successfully.
TEST_F(ScanServiceTest, PdfScan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  const std::vector<std::string> scan_data = {CreatePng(), CreatePng(),
                                              CreatePng()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  base::Time::Exploded scan_time;
  // Since we're using mock time, this is deterministic.
  base::Time::Now().LocalExplode(&scan_time);

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kPdf);
  const base::FilePath saved_scan_path =
      CreateSavedPdfScanPath(temp_dir_.GetPath(), scan_time);
  EXPECT_FALSE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_TRUE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kSuccess,
            fake_scan_job_observer_.scan_result());
  const std::vector<base::FilePath> scanned_file_paths =
      fake_scan_job_observer_.scanned_file_paths();
  EXPECT_EQ(1u, scanned_file_paths.size());
  EXPECT_EQ(saved_scan_path, scanned_file_paths.front());
}

// Test that an Epson ADF Duplex scan, which produces flipped pages, completes
// successfully.
TEST_F(ScanServiceTest, RotateEpsonADF) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse({kEpsonTestName});
  fake_lorgnette_scanner_manager_.SetGetScannerCapabilitiesResponse(
      CreateEpsonScannerCapabilities());
  const std::vector<std::string> scan_data = {CreatePng(), CreatePng(),
                                              CreatePng()};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  base::Time::Exploded scan_time;
  // Since we're using mock time, this is deterministic.
  base::Time::Now().LocalExplode(&scan_time);

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kPdf);
  const base::FilePath saved_scan_path =
      CreateSavedPdfScanPath(temp_dir_.GetPath(), scan_time);
  EXPECT_FALSE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_TRUE(base::PathExists(saved_scan_path));
  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  const std::vector<base::FilePath> scanned_file_paths =
      fake_scan_job_observer_.scanned_file_paths();
  EXPECT_EQ(1u, scanned_file_paths.size());
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

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  const mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kPng);

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kDeviceBusy,
            fake_scan_job_observer_.scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());
}

// Test that when a page fails to save during the scan, the scan job is marked
// as failed.
TEST_F(ScanServiceTest, PageSaveFails) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  // Sending an empty string in test data simulates a page saving to fail.
  const std::vector<std::string> scan_data = {"TestData1", "", "TestData3"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  const mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kJpg);

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kUnknownError,
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

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  const mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kPng);

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kDeviceBusy,
            fake_scan_job_observer_.scan_result());
  EXPECT_TRUE(fake_scan_job_observer_.scanned_file_paths().empty());

  // Set scan data so next scan is successful.
  const std::vector<std::string> scan_data = {"TestData1", "TestData2",
                                              "TestData3"};
  fake_lorgnette_scanner_manager_.SetScanResponse(scan_data);
  base::Time::Exploded scan_time;
  // Since we're using mock time, this is deterministic.
  base::Time::Now().LocalExplode(&scan_time);

  const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
      temp_dir_.GetPath(), scan_time, "png", scan_data.size());
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_TRUE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kSuccess,
            fake_scan_job_observer_.scan_result());
  EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());
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

  base::Time::Exploded scan_time;
  // Since we're using mock time, this is deterministic.
  base::Time::Now().LocalExplode(&scan_time);

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  const mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kPng);
  const std::vector<base::FilePath> saved_scan_paths = CreateSavedScanPaths(
      temp_dir_.GetPath(), scan_time, "png", scan_data.size());
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_FALSE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  for (const auto& saved_scan_path : saved_scan_paths)
    EXPECT_TRUE(base::PathExists(saved_scan_path));

  EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kSuccess,
            fake_scan_job_observer_.scan_result());
  EXPECT_EQ(saved_scan_paths, fake_scan_job_observer_.scanned_file_paths());

  // Remove the scan data from FakeLorgnetteScannerManager so the scan will
  // fail.
  fake_lorgnette_scanner_manager_.SetScanResponse({});

  EXPECT_TRUE(StartScan(scanners[0]->id, settings.Clone()));
  EXPECT_FALSE(fake_scan_job_observer_.scan_success());
  EXPECT_EQ(mojo_ipc::ScanResult::kDeviceBusy,
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

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  const mojo_ipc::ScanSettings settings =
      CreateScanSettings(temp_dir_.GetPath(), mojo_ipc::FileType::kPng);

  StartScan(scanners[0]->id, settings.Clone());
  CancelScan();
  EXPECT_TRUE(fake_scan_job_observer_.cancel_scan_success());
}

}  // namespace ash
