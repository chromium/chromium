// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"
#include "chromeos/components/scanning/mojom/scanning.mojom-test-utils.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

// Relative path where scanned images are saved, relative to the root directory.
constexpr char kMyFilesPath[] = "home/chronos/user/MyFiles";

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

class ScanServiceTest : public testing::Test {
 public:
  ScanServiceTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::CreateDirectory(temp_dir_.GetPath().Append(kMyFilesPath)));
    scan_service_.SetRootDirForTesting(temp_dir_.GetPath());
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
  // |settings| by calling ScanService::Scan() via the mojo::Remote.
  bool Scan(const base::UnguessableToken& scanner_id,
            mojo_ipc::ScanSettingsPtr settings) {
    bool success;
    mojo_ipc::ScanServiceAsyncWaiter(scan_service_remote_.get())
        .Scan(scanner_id, std::move(settings), &success);
    return success;
  }

 protected:
  FakeLorgnetteScannerManager fake_lorgnette_scanner_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  ScanService scan_service_{&fake_lorgnette_scanner_manager_};
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

// Test that a scan can be performed successfully.
TEST_F(ScanServiceTest, Scan) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  fake_lorgnette_scanner_manager_.SetScanResponse("TestData");
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);
  EXPECT_TRUE(Scan(scanners[0]->id, mojo_ipc::ScanSettings::New()));
}

}  // namespace chromeos
