// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scanning_type_converters.h"

#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

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
};

// Document source name used for tests.
constexpr char kDocumentSourceName[] = "Test Name";

// Resolutions used for tests.
constexpr uint32_t kFirstResolution = 75;
constexpr uint32_t kSecondResolution = 300;

// Returns a DocumentSource object with the given |source_type|.
lorgnette::DocumentSource CreateLorgnetteDocumentSource(
    lorgnette::SourceType source_type) {
  lorgnette::DocumentSource source;
  source.set_type(source_type);
  source.set_name(kDocumentSourceName);
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

// Returns a ScanSettingsPtr with the given |color_mode|.
mojo_ipc::ScanSettingsPtr CreateMojomScanSettings(
    mojo_ipc::ColorMode color_mode) {
  mojo_ipc::ScanSettings settings;
  settings.source_name = kDocumentSourceName;
  settings.color_mode = color_mode;
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
      mojo::ConvertTo<lorgnette::ScanSettings>(
          CreateMojomScanSettings(params().mojom_color_mode));
  EXPECT_EQ(lorgnette_settings.source_name(), kDocumentSourceName);
  EXPECT_EQ(lorgnette_settings.color_mode(), params().lorgnette_color_mode);
  EXPECT_EQ(lorgnette_settings.resolution(), kFirstResolution);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ScanSettingsTest,
    testing::Values(ScanSettingsTestParams{mojo_ipc::ColorMode::kBlackAndWhite,
                                           lorgnette::MODE_LINEART},
                    ScanSettingsTestParams{mojo_ipc::ColorMode::kGrayscale,
                                           lorgnette::MODE_GRAYSCALE},
                    ScanSettingsTestParams{mojo_ipc::ColorMode::kColor,
                                           lorgnette::MODE_COLOR}));

}  // namespace chromeos
