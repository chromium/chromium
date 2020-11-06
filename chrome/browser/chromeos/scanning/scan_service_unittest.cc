// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"
#include "chromeos/components/scanning/mojom/scanning.mojom-test-utils.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

// Path to the user's "My files" folder.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";

// Scanner names used for tests.
constexpr char kFirstTestScannerName[] = "Test Scanner 1";
constexpr char kSecondTestScannerName[] = "Test Scanner 2";

// Document source name used for tests.
constexpr char kDocumentSourceName[] = "Flatbed";

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

// Returns a ScannerCapabilities object.
lorgnette::ScannerCapabilities CreateLorgnetteScannerCapabilities() {
  lorgnette::ScannerCapabilities caps;
  *caps.add_sources() = CreateLorgnetteDocumentSource();
  caps.add_color_modes(lorgnette::MODE_COLOR);
  caps.add_resolutions(kFirstResolution);
  caps.add_resolutions(kSecondResolution);
  return caps;
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

  void OnScanComplete(bool success) override { scan_success_ = success; }

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
    return progress_ == 100 && page_complete_ && scan_success_;
  }

 private:
  uint32_t progress_ = 0;
  bool page_complete_ = false;
  bool scan_success_ = false;
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

  // Performs a scan with the scanner identified by |scanner_id| with the given
  // |settings| by calling ScanService::StartScan() via the mojo::Remote.
  bool Scan(const base::UnguessableToken& scanner_id,
            mojo_ipc::ScanSettingsPtr settings) {
    bool success;
    mojo_ipc::ScanServiceAsyncWaiter(scan_service_remote_.get())
        .StartScan(scanner_id, std::move(settings),
                   fake_scan_job_observer_.GenerateRemote(), &success);
    scan_service_remote_.FlushForTesting();
    return success;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  FakeLorgnetteScannerManager fake_lorgnette_scanner_manager_;
  FakeScanJobObserver fake_scan_job_observer_;
  ScanService scan_service_{&fake_lorgnette_scanner_manager_, base::FilePath(),
                            base::FilePath()};

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  EXPECT_FALSE(
      Scan(base::UnguessableToken::Create(), mojo_ipc::ScanSettings::New()));
}

// Test that attempting to scan with an unsupported file path fails.
// Specifically, use a file path with directory navigation (e.g. "..") to verify
// it can't be used to save scanned images to an unsupported path.
TEST_F(ScanServiceTest, ScanWithUnsupportedFilePath) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  fake_lorgnette_scanner_manager_.SetScanResponse("TestData");
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  const base::FilePath my_files_path(kMyFilesPath);
  scan_service_.SetMyFilesPathForTesting(my_files_path);
  mojo_ipc::ScanSettings settings;
  settings.scan_to_path = my_files_path.Append("../../../var/log");
  settings.file_type = mojo_ipc::FileType::kPng;
  EXPECT_FALSE(Scan(scanners[0]->id, settings.Clone()));
}

// Test that a scan can be performed successfully.
TEST_F(ScanServiceTest, Scan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  fake_lorgnette_scanner_manager_.SetScanResponse("TestData");
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);

  base::Time::Exploded scan_time;
  // Since we're using mock time, this is deterministic.
  base::Time::Now().LocalExplode(&scan_time);

  scan_service_.SetMyFilesPathForTesting(temp_dir_.GetPath());
  mojo_ipc::ScanSettings settings;
  settings.scan_to_path = temp_dir_.GetPath();
  std::map<std::string, mojo_ipc::FileType> file_types = {
      {"png", mojo_ipc::FileType::kPng}, {"jpg", mojo_ipc::FileType::kJpg}};
  base::FilePath saved_scan_path;
  for (const auto& type : file_types) {
    saved_scan_path = temp_dir_.GetPath().Append(base::StringPrintf(
        "scan_%02d%02d%02d-%02d%02d%02d_1.%s", scan_time.year, scan_time.month,
        scan_time.day_of_month, scan_time.hour, scan_time.minute,
        scan_time.second, type.first.c_str()));
    EXPECT_FALSE(base::PathExists(saved_scan_path));

    settings.file_type = type.second;
    EXPECT_TRUE(Scan(scanners[0]->id, settings.Clone()));
    EXPECT_TRUE(base::PathExists(saved_scan_path));
    EXPECT_TRUE(fake_scan_job_observer_.scan_success());
  }
}

}  // namespace chromeos
