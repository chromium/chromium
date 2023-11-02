// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/mojom/scanning_type_converters.h"
#include <map>
#include "base/containers/fixed_flat_map.h"

#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::testing::ElementsAreArray;

using MojomScanResult = ash::scanning::mojom::ScanResult;
using ProtoScanFailureMode = lorgnette::ScanFailureMode;

using MojomColorMode = ash::scanning::mojom::ColorMode;
using ProtoColorMode = lorgnette::ColorMode;

using MojomSourceType = ash::scanning::mojom::SourceType;
using ProtoSourceType = lorgnette::SourceType;

using MojomFileType = ash::scanning::mojom::FileType;
using ProtoImageFormat = lorgnette::ImageFormat;

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
  mojo_ipc::FileType mojom_file_type;
  lorgnette::ImageFormat lorgnete_image_format;
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
    lorgnette::SourceType source_type,
    lorgnette::ColorMode color_mode) {
  lorgnette::DocumentSource source;
  source.set_type(source_type);
  source.set_name(kDocumentSourceName);
  source.mutable_area()->set_width(kScanAreaWidthMm);
  source.mutable_area()->set_height(kScanAreaHeightMm);
  source.add_color_modes(color_mode);
  source.add_resolutions(kFirstResolution);
  source.add_resolutions(kSecondResolution);
  return source;
}

// Returns a ScannerCapabilities object with the given |source_type| and
// |color_mode|.
lorgnette::ScannerCapabilities CreateLorgnetteScannerCapabilities(
    lorgnette::SourceType source_type,
    lorgnette::ColorMode color_mode) {
  lorgnette::ScannerCapabilities caps;
  *caps.add_sources() = CreateLorgnetteDocumentSource(source_type, color_mode);
  return caps;
}

// Returns a ScanSettingsPtr with the given |color_mode|, |page_size| and
// |file_type|.
mojo_ipc::ScanSettingsPtr CreateMojomScanSettings(
    mojo_ipc::ColorMode color_mode,
    mojo_ipc::PageSize page_size,
    mojo_ipc::FileType file_type) {
  mojo_ipc::ScanSettings settings;
  settings.source_name = kDocumentSourceName;
  settings.color_mode = color_mode;
  settings.file_type = file_type;
  settings.page_size = page_size;
  settings.resolution_dpi = kFirstResolution;
  return settings.Clone();
}

template <typename MojoEnum, typename SourceEnum, size_t N>
void TestToMojom(const base::fixed_flat_map<MojoEnum, SourceEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size() - 1, static_cast<size_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    EXPECT_EQ(
        enum_pair.first,
        (mojo::EnumTraits<MojoEnum, SourceEnum>::ToMojom(enum_pair.second)));
  }
}

template <typename MojoEnum, typename SourceEnum, size_t N>
void TestFromMojom(const base::fixed_flat_map<MojoEnum, SourceEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size() - 1, static_cast<uint32_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    SourceEnum mojo_to_source;
    EXPECT_TRUE((mojo::EnumTraits<MojoEnum, SourceEnum>::FromMojom(
        enum_pair.first, &mojo_to_source)));
    EXPECT_EQ(mojo_to_source, enum_pair.second);
  }
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
      mojo::StructTraits<ash::scanning::mojom::ScannerCapabilitiesPtr,
                         lorgnette::ScannerCapabilities>::
          ToMojom(CreateLorgnetteScannerCapabilities(
              params().lorgnette_source_type, params().lorgnette_color_mode));
  ASSERT_EQ(mojo_caps->sources.size(), 1u);
  EXPECT_EQ(mojo_caps->sources[0]->type, params().mojom_source_type);
  EXPECT_EQ(mojo_caps->sources[0]->name, kDocumentSourceName);
  EXPECT_THAT(mojo_caps->sources[0]->page_sizes,
              ElementsAreArray(
                  {mojo_ipc::PageSize::kMax, mojo_ipc::PageSize::kIsoA3,
                   mojo_ipc::PageSize::kIsoA4, mojo_ipc::PageSize::kIsoB4,
                   mojo_ipc::PageSize::kLegal, mojo_ipc::PageSize::kNaLetter,
                   mojo_ipc::PageSize::kTabloid}));
  ASSERT_EQ(mojo_caps->sources[0]->color_modes.size(), 1u);
  EXPECT_EQ(mojo_caps->sources[0]->color_modes[0], params().mojom_color_mode);
  ASSERT_EQ(mojo_caps->sources[0]->resolutions.size(), 2u);
  EXPECT_THAT(mojo_caps->sources[0]->resolutions,
              ElementsAreArray({kFirstResolution, kSecondResolution}));
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
// * |mojom_file_type| - the mojo_ipc::FileType to convert.
// * |lorgnette_image_format| - the expected lorgnette::ImageFormat.
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
      mojo::StructTraits<lorgnette::ScanSettings, mojo_ipc::ScanSettingsPtr>::
          ToMojom(CreateMojomScanSettings(params().mojom_color_mode,
                                          params().mojom_page_size,
                                          params().mojom_file_type));
  EXPECT_EQ(lorgnette_settings.source_name(), kDocumentSourceName);
  EXPECT_EQ(lorgnette_settings.color_mode(), params().lorgnette_color_mode);
  EXPECT_EQ(lorgnette_settings.image_format(), params().lorgnete_image_format);
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
    testing::Values(
        ScanSettingsTestParams{
            mojo_ipc::ColorMode::kBlackAndWhite, lorgnette::MODE_LINEART,
            mojo_ipc::FileType::kPng, lorgnette::IMAGE_FORMAT_PNG,
            mojo_ipc::PageSize::kIsoA4, 210, 297},
        ScanSettingsTestParams{
            mojo_ipc::ColorMode::kGrayscale, lorgnette::MODE_GRAYSCALE,
            mojo_ipc::FileType::kJpg, lorgnette::IMAGE_FORMAT_JPEG,
            mojo_ipc::PageSize::kNaLetter, 215.9, 279.4},
        ScanSettingsTestParams{mojo_ipc::ColorMode::kColor,
                               lorgnette::MODE_COLOR, mojo_ipc::FileType::kPdf,
                               lorgnette::IMAGE_FORMAT_JPEG,
                               mojo_ipc::PageSize::kMax, 0, 0}));

// Test that mapping between lorgnette::ColorMode and mojo_ipc::ColorMode
// behaves as expected.
TEST(ScanningMojomTraitsTest, ColorMode) {
  constexpr auto enums =
      base::MakeFixedFlatMap<MojomColorMode, ProtoColorMode>({
          {MojomColorMode::kBlackAndWhite, ProtoColorMode::MODE_LINEART},
          {MojomColorMode::kGrayscale, ProtoColorMode::MODE_GRAYSCALE},
          {MojomColorMode::kColor, ProtoColorMode::MODE_COLOR},
      });

  TestToMojom(enums);
  TestFromMojom(enums);
}

// Test that mapping between lorgnette::SourceType and mojo_ipc::SourceType
// behaves as expected.
TEST(ScanningMojomTraitsTest, SourceType) {
  constexpr auto enums =
      base::MakeFixedFlatMap<MojomSourceType, ProtoSourceType>({
          {MojomSourceType::kFlatbed, ProtoSourceType::SOURCE_PLATEN},
          {MojomSourceType::kAdfSimplex, ProtoSourceType::SOURCE_ADF_SIMPLEX},
          {MojomSourceType::kAdfDuplex, ProtoSourceType::SOURCE_ADF_DUPLEX},
          {MojomSourceType::kDefault, ProtoSourceType::SOURCE_DEFAULT},
          {MojomSourceType::kUnknown, ProtoSourceType::SOURCE_UNSPECIFIED},
      });

  TestToMojom(enums);
  TestFromMojom(enums);
}

TEST(ScanningMojomTraitsTest, FileType) {
  constexpr auto enums =
      base::MakeFixedFlatMap<MojomFileType, ProtoImageFormat>({
          {MojomFileType::kPng, ProtoImageFormat::IMAGE_FORMAT_PNG},
          {MojomFileType::kJpg, ProtoImageFormat::IMAGE_FORMAT_JPEG},
      });
  // The mojo enum is sparse - there is no pdf format in lorgnette::ImageFormat.
  // Test without calling TestToMojom(enums), TestFromMojom(enums) functions.
  // Test ToMojom
  for (auto enum_pair : enums) {
    EXPECT_EQ(enum_pair.first,
              (mojo::EnumTraits<MojomFileType, ProtoImageFormat>::ToMojom(
                  enum_pair.second)));
  }

  // Test FromMojom
  for (auto enum_pair : enums) {
    ProtoImageFormat mojo_to_source;
    EXPECT_TRUE((mojo::EnumTraits<MojomFileType, ProtoImageFormat>::FromMojom(
        enum_pair.first, &mojo_to_source)));
    EXPECT_EQ(mojo_to_source, enum_pair.second);
  }
}

// Test that mapping between lorgnette::ScanFailureMode and mojo_ipc::ScanResult
// behaves as expected.
TEST(ScanningMojomTraitsTest, ScanResult) {
  constexpr auto enums =
      base::MakeFixedFlatMap<MojomScanResult, ProtoScanFailureMode>({
          {MojomScanResult::kSuccess,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE},
          {MojomScanResult::kUnknownError,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN},
          {MojomScanResult::kDeviceBusy,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY},
          {MojomScanResult::kAdfJammed,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_ADF_JAMMED},
          {MojomScanResult::kAdfEmpty,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_ADF_EMPTY},
          {MojomScanResult::kFlatbedOpen,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_FLATBED_OPEN},
          {MojomScanResult::kIoError,
           ProtoScanFailureMode::SCAN_FAILURE_MODE_IO_ERROR},
      });

  TestToMojom(enums);
  TestFromMojom(enums);
}

}  // namespace ash
