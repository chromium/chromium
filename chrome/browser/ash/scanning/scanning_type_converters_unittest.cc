// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scanning_type_converters.h"

#include "ash/content/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::testing::ElementsAreArray;

namespace mojo_ipc = scanning::mojom;

// POD struct for ScannerCapabilitiesTest.
struct ScannerCapabilitiesTestParams {
  lorgnette::SourceType lorgnette_source_type;
  lorgnette::ColorMode lorgnette_color_mode;
  mojo_ipc::SourceType mojom_source_type;
  mojo_ipc::ColorMode mojom_color_mode;
};

// POD struct for ScanSettingsTest.
struct ScanSettingsTestParams {
  mojo_ipc::ColorMode mojom_color_mode;
  lorgnette::ColorMode lorgnette_color_mode;
  mojo_ipc::PageSize mojom_page_size;
  double bottom_right_x;
  double bottom_right_y;
};

// Document source name used for tests.
constexpr char kDocumentSourceName[] = "Test Name";

// Scannable area dimensions used for tests. These are large enough to ensure
// every page size is supported by the scanner.
constexpr int kScanAreaWidthMm = 500;
constexpr int kScanAreaHeightMm = 750;

// Resolutions used for tests.
constexpr uint32_t kFirstResolution = 75;
constexpr uint32_t kSecondResolution = 300;

// Returns a DocumentSource object with the given |source_type|.
lorgnette::DocumentSource CreateLorgnetteDocumentSource(
    lorgnette::SourceType source_type) {
  lorgnette::DocumentSource source;
  source.set_type(source_type);
  source.set_name(kDocumentSourceName);
  source.mutable_area()->set_width(kScanAreaWidthMm);
  source.mutable_area()->set_height(kScanAreaHeightMm);
  return source;
}

// Returns a ScannerCapabilities object with the given |source_type| and
// |color_mode|.
lorgnette::ScannerCapabilities CreateLorgnetteScannerCapabilities(
    lorgnette::SourceType source_type,
    lorgnette::ColorMode color_mode) {
  lorgnette::ScannerCapabilities caps;
  *caps.add_sources() = CreateLorgnetteDocumentSource(source_type);
  caps.add_color_modes(color_mode);
  caps.add_resolutions(kFirstResolution);
  caps.add_resolutions(kSecondResolution);
  return caps;
}

// Returns a ScanSettingsPtr with the given |color_mode| and |page_size|.
mojo_ipc::ScanSettingsPtr CreateMojomScanSettings(
    mojo_ipc::ColorMode color_mode,
    mojo_ipc::PageSize page_size) {
  mojo_ipc::ScanSettings settings;
  settings.source_name = kDocumentSourceName;
  settings.color_mode = color_mode;
  settings.page_size = page_size;
  settings.resolution_dpi = kFirstResolution;
  return settings.Clone();
}

}  // namespace

// Tests that each possible lorgnette::ScannerCapabilities proto can be
// correctly converted into a mojo_ipc::ScannerCapabilitiesPtr.
//
// This is a parameterized test with the following parameters (accessed through
// ScannerCapabilitiesTestParams):
// * |lorgnette_source_type| - the lorgnette::SourceType to convert.
// * |lorgnette_color_mode| - the lorgnette::ColorMode to convert.
// * |mojom_source_type| - the expected mojo_ipc::SourceType.
// * |mojom_color_mode| - the expected mojo_ipc::ColorMode.
class ScannerCapabilitiesTest
    : public testing::Test,
      public testing::WithParamInterface<ScannerCapabilitiesTestParams> {
 protected:
  // Accessors to the test parameters returned by gtest's GetParam():
  ScannerCapabilitiesTestParams params() const { return GetParam(); }
};

// Test that lorgnette::ScannerCapabilities can be converted into a
// mojo_ipc::ScannerCapabilitiesPtr.
TEST_P(ScannerCapabilitiesTest, LorgnetteCapsToMojom) {
  mojo_ipc::ScannerCapabilitiesPtr mojo_caps =
      mojo::ConvertTo<mojo_ipc::ScannerCapabilitiesPtr>(
          CreateLorgnetteScannerCapabilities(params().lorgnette_source_type,
                                             params().lorgnette_color_mode));
  ASSERT_EQ(mojo_caps->sources.size(), 1u);
  EXPECT_EQ(mojo_caps->sources[0]->type, params().mojom_source_type);
  EXPECT_EQ(mojo_caps->sources[0]->name, kDocumentSourceName);
  EXPECT_THAT(
      mojo_caps->sources[0]->page_sizes,
      ElementsAreArray({mojo_ipc::PageSize::kMax, mojo_ipc::PageSize::kIsoA4,
                        mojo_ipc::PageSize::kNaLetter}));
  ASSERT_EQ(mojo_caps->color_modes.size(), 1u);
  EXPECT_EQ(mojo_caps->color_modes[0], params().mojom_color_mode);
  ASSERT_EQ(mojo_caps->resolutions.size(), 2u);
  EXPECT_EQ(mojo_caps->resolutions[0], kFirstResolution);
  EXPECT_EQ(mojo_caps->resolutions[1], kSecondResolution);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ScannerCapabilitiesTest,
    testing::Values(
        ScannerCapabilitiesTestParams{lorgnette::SOURCE_PLATEN,
                                      lorgnette::MODE_LINEART,
                                      mojo_ipc::SourceType::kFlatbed,
                                      mojo_ipc::ColorMode::kBlackAndWhite},
        ScannerCapabilitiesTestParams{
            lorgnette::SOURCE_ADF_SIMPLEX, lorgnette::MODE_GRAYSCALE,
            mojo_ipc::SourceType::kAdfSimplex, mojo_ipc::ColorMode::kGrayscale},
        ScannerCapabilitiesTestParams{
            lorgnette::SOURCE_ADF_DUPLEX, lorgnette::MODE_COLOR,
            mojo_ipc::SourceType::kAdfDuplex, mojo_ipc::ColorMode::kColor},
        ScannerCapabilitiesTestParams{
            lorgnette::SOURCE_DEFAULT, lorgnette::MODE_COLOR,
            mojo_ipc::SourceType::kDefault, mojo_ipc::ColorMode::kColor}));

// Tests that each possible mojo_ipc::ScanSettingsPtr can be correctly converted
// into a lorgnette::ScanSettings proto.
//
// This is a parameterized test with the following parameters (accessed through
// ScanSettingsTestParams):
// * |mojom_color_mode| - the mojo_ipc::ColorMode to convert.
// * |lorgnette_color_mode| - the expected lorgnette::ColorMode.
// * |mojom_page_size| - the mojo_ipc::PageSize to convert.
// * |bottom_right_x| - the expected bottom-right x-coordinate.
// * |bottom_right_y| - the expected bottom-right y-coordinate.
class ScanSettingsTest
    : public testing::Test,
      public testing::WithParamInterface<ScanSettingsTestParams> {
 protected:
  // Accessors to the test parameters returned by gtest's GetParam():
  ScanSettingsTestParams params() const { return GetParam(); }
};

// Test that mojo_ipc::ScanSettingsPtr can be converted into a
// lorgnette::ScanSettings proto.
TEST_P(ScanSettingsTest, MojomSettingsToLorgnette) {
  lorgnette::ScanSettings lorgnette_settings =
      mojo::ConvertTo<lorgnette::ScanSettings>(CreateMojomScanSettings(
          params().mojom_color_mode, params().mojom_page_size));
  EXPECT_EQ(lorgnette_settings.source_name(), kDocumentSourceName);
  EXPECT_EQ(lorgnette_settings.color_mode(), params().lorgnette_color_mode);
  EXPECT_EQ(lorgnette_settings.resolution(), kFirstResolution);

  if (params().mojom_page_size == mojo_ipc::PageSize::kMax) {
    EXPECT_FALSE(lorgnette_settings.has_scan_region());
  } else {
    ASSERT_TRUE(lorgnette_settings.has_scan_region());
    EXPECT_EQ(lorgnette_settings.scan_region().top_left_x(), 0);
    EXPECT_EQ(lorgnette_settings.scan_region().top_left_y(), 0);
    EXPECT_EQ(lorgnette_settings.scan_region().bottom_right_x(),
              params().bottom_right_x);
    EXPECT_EQ(lorgnette_settings.scan_region().bottom_right_y(),
              params().bottom_right_y);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ScanSettingsTest,
    testing::Values(ScanSettingsTestParams{mojo_ipc::ColorMode::kBlackAndWhite,
                                           lorgnette::MODE_LINEART,
                                           mojo_ipc::PageSize::kIsoA4, 210,
                                           297},
                    ScanSettingsTestParams{mojo_ipc::ColorMode::kGrayscale,
                                           lorgnette::MODE_GRAYSCALE,
                                           mojo_ipc::PageSize::kNaLetter, 215.9,
                                           279.4},
                    ScanSettingsTestParams{mojo_ipc::ColorMode::kColor,
                                           lorgnette::MODE_COLOR,
                                           mojo_ipc::PageSize::kMax, 0, 0}));

// Test that each lorgnette::ScanFailureMode is converted into the correct
// mojo_ipc::ScanResult.
TEST(ScanResultTest, Convert) {
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_NO_FAILURE),
            mojo_ipc::ScanResult::kSuccess);
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_UNKNOWN),
            mojo_ipc::ScanResult::kUnknownError);
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY),
            mojo_ipc::ScanResult::kDeviceBusy);
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED),
            mojo_ipc::ScanResult::kAdfJammed);
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_ADF_EMPTY),
            mojo_ipc::ScanResult::kAdfEmpty);
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_FLATBED_OPEN),
            mojo_ipc::ScanResult::kFlatbedOpen);
  EXPECT_EQ(mojo::ConvertTo<mojo_ipc::ScanResult>(
                lorgnette::SCAN_FAILURE_MODE_IO_ERROR),
            mojo_ipc::ScanResult::kIoError);
}

}  // namespace ash
