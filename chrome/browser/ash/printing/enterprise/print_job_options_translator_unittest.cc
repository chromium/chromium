// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/print_job_options_translator.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/protobuf_matchers.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/print_job_options.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::EqualsProto;
using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using ::testing::ElementsAre;

namespace chromeos {

namespace {

// Explicitly declaring base::Value::Dict is not very convenient and is much
// less readable compared to JSON. Thus using JSONReader to convert JSON to
// base::Value::Dict.
base::Value::Dict ConvertJsonToDict(const char json[]) {
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(value_with_error.has_value())
      << "Failed to parse print options (" << value_with_error.error().message
      << ") on line " << value_with_error.error().line << " at position "
      << value_with_error.error().column;
  return std::move(value_with_error->GetDict());
}

TEST(ManagedPrintOptionsProtoFromDict, EmptyPrintJobOptions) {
  PrintJobOptions print_job_option_parsed =
      ManagedPrintOptionsProtoFromDict(base::Value::Dict());
  EXPECT_THAT(print_job_option_parsed,
              EqualsProto(PrintJobOptions::default_instance()));
}

TEST(ManagedPrintOptionsProtoFromDict, MultipleValidPrintJobOptions) {
  constexpr static char kPrintJobOptions[] = R"json(
    {
      "media_size": {
        "default_value": {
          "width": 60,
          "height": 60
        },
        "allowed_values": [
          {
            "width": 60,
            "height": 60
          },
          {
            "width": 90,
            "height": 90
          }
        ]
      },
      "media_type": {
        "default_value": "paper",
        "allowed_values": ["paper", "metal", "wood"]
      },
      "duplex": {
        "default_value": 3,
        "allowed_values": [2, 3]
      },
      "color": { "default_value": true },
      "dpi": {
        "allowed_values": [
          { "horizontal": 600, "vertical": 600 },
          { "horizontal": 900, "vertical": 900 }
        ]
      },
      "quality": {
        "default_value": 1,
        "allowed_values": [1]
      },
      "print_as_image": {
        "default_value": false,
        "allowed_values": [false, true]
      }
    }
  )json";
  base::Value::Dict print_job_options_dict =
      ConvertJsonToDict(kPrintJobOptions);

  PrintJobOptions print_job_options =
      ManagedPrintOptionsProtoFromDict(print_job_options_dict);

  // Media size
  SizeOption expected_media_size;
  expected_media_size.mutable_default_value()->set_width(60);
  expected_media_size.mutable_default_value()->set_height(60);
  std::vector<Size> media_size_available_values(2);
  media_size_available_values[0].set_width(60);
  media_size_available_values[0].set_height(60);
  media_size_available_values[1].set_width(90);
  media_size_available_values[1].set_height(90);
  *expected_media_size.mutable_allowed_values() = RepeatedPtrField<Size>(
      media_size_available_values.begin(), media_size_available_values.end());
  EXPECT_THAT(print_job_options.media_size(), EqualsProto(expected_media_size));

  // Media type
  StringOption expected_media_type;
  expected_media_type.set_default_value("paper");
  std::vector<std::string> media_type_available_values(3);
  media_type_available_values[0] = "paper";
  media_type_available_values[1] = "metal";
  media_type_available_values[2] = "wood";
  *expected_media_type.mutable_allowed_values() = RepeatedPtrField<std::string>(
      media_type_available_values.begin(), media_type_available_values.end());
  EXPECT_THAT(print_job_options.media_type(), EqualsProto(expected_media_type));

  // Duplex
  DuplexOption expected_duplex;
  expected_duplex.set_default_value(DuplexType::DUPLEX_LONG_EDGE);
  std::vector<int> duplex_available_values(2);
  duplex_available_values[0] = DuplexType::DUPLEX_SHORT_EDGE;
  duplex_available_values[1] = DuplexType::DUPLEX_LONG_EDGE;
  *expected_duplex.mutable_allowed_values() = RepeatedField<int>(
      duplex_available_values.begin(), duplex_available_values.end());
  EXPECT_THAT(print_job_options.duplex(), EqualsProto(expected_duplex));

  // Color
  BoolOption expected_color;
  expected_color.set_default_value(true);
  EXPECT_THAT(print_job_options.color(), EqualsProto(expected_color));

  // Dpi
  DPIOption expected_dpi;
  std::vector<DPI> dpi_available_values(2);
  dpi_available_values[0].set_horizontal(600);
  dpi_available_values[0].set_vertical(600);
  dpi_available_values[1].set_horizontal(900);
  dpi_available_values[1].set_vertical(900);
  *expected_dpi.mutable_allowed_values() = RepeatedPtrField<DPI>(
      dpi_available_values.begin(), dpi_available_values.end());
  EXPECT_THAT(print_job_options.dpi(), EqualsProto(expected_dpi));

  // Quality
  QualityOption expected_quality;
  expected_quality.set_default_value(QualityType::QUALITY_DRAFT);
  std::vector<int> quality_available_values(1);
  quality_available_values[0] = QualityType::QUALITY_DRAFT;
  *expected_quality.mutable_allowed_values() = RepeatedField<int>(
      quality_available_values.begin(), quality_available_values.end());
  EXPECT_THAT(print_job_options.quality(), EqualsProto(expected_quality));

  // Print as image
  BoolOption expected_print_as_image;
  expected_print_as_image.set_default_value(false);
  std::vector<bool> print_as_image_available_values(2);
  print_as_image_available_values[0] = false;
  print_as_image_available_values[1] = true;
  *expected_print_as_image.mutable_allowed_values() =
      RepeatedField<bool>(print_as_image_available_values.begin(),
                          print_as_image_available_values.end());
  EXPECT_THAT(print_job_options.print_as_image(),
              EqualsProto(expected_print_as_image));
}

TEST(ManagedPrintOptionsProtoFromDict, InvalidPrintJobOptions) {
  // This json file has several problems, but the parser shouldn't crash. We'll
  // verify correctness of data at later conversion stages.
  constexpr static char kPrintJobOptions[] = R"json(
    {
      "media_size": {
        "default_value": {
          "width": "not_a_number",
          "height": "not_a_number"
        },
        "allowed_values": "not_a_list"
      },
      "media_type": {
        "default_value": 42,
        "allowed_values": [42, "paper"]
      },
      "duplex": {
        "default_value": 1234567,
        "allowed_values": []
      }
    }
  )json";

  base::Value::Dict print_job_options_dict =
      ConvertJsonToDict(kPrintJobOptions);

  PrintJobOptions print_job_options =
      ManagedPrintOptionsProtoFromDict(print_job_options_dict);
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, InvalidMediaSize) {
  PrintJobOptions print_job_options;
  // Default value of "media size" option doesn't have width.
  print_job_options.mutable_media_size()->mutable_default_value()->set_height(
      32);

  ASSERT_EQ(ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options),
            std::nullopt);
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, InvalidDpi) {
  PrintJobOptions print_job_options;
  // Default value of "Dpi" option doesn't have horizontal value.
  print_job_options.mutable_dpi()->mutable_default_value()->set_vertical(1000);

  ASSERT_EQ(ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options),
            std::nullopt);
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, MediaSize) {
  SizeOption media_size;
  media_size.mutable_default_value()->set_width(60);
  media_size.mutable_default_value()->set_height(60);
  std::vector<Size> media_size_available_values(2);
  media_size_available_values[0].set_width(60);
  media_size_available_values[0].set_height(60);
  media_size_available_values[1].set_width(90);
  media_size_available_values[1].set_height(90);
  *media_size.mutable_allowed_values() = RepeatedPtrField<Size>(
      media_size_available_values.begin(), media_size_available_values.end());
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_media_size() = media_size;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->media_size.default_value.has_value());
  EXPECT_EQ(print_job_options->media_size.default_value.value(),
            (Printer::Size{60, 60}));
  EXPECT_THAT(print_job_options->media_size.allowed_values,
              ElementsAre(Printer::Size{60, 60}, Printer::Size{90, 90}));
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, MediaType) {
  StringOption media_type;
  media_type.set_default_value("paper");
  media_type.add_allowed_values("paper");
  media_type.add_allowed_values("metal");
  media_type.add_allowed_values("wood");
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_media_type() = media_type;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->media_type.default_value.has_value());
  EXPECT_EQ(print_job_options->media_type.default_value.value(), "paper");
  EXPECT_THAT(print_job_options->media_type.allowed_values,
              ElementsAre("paper", "metal", "wood"));
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, Duplex) {
  DuplexOption duplex;
  duplex.set_default_value(DuplexType::DUPLEX_LONG_EDGE);
  duplex.add_allowed_values(DuplexType::DUPLEX_LONG_EDGE);
  duplex.add_allowed_values(DuplexType::DUPLEX_SHORT_EDGE);
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_duplex() = duplex;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->duplex.default_value.has_value());
  EXPECT_EQ(print_job_options->duplex.default_value.value(),
            Printer::DuplexType::kLongEdge);
  EXPECT_THAT(print_job_options->duplex.allowed_values,
              ElementsAre(Printer::DuplexType::kLongEdge,
                          Printer::DuplexType::kShortEdge));
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, Color) {
  BoolOption color;
  color.set_default_value(true);
  color.add_allowed_values(true);
  color.add_allowed_values(false);
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_color() = color;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->color.default_value.has_value());
  EXPECT_EQ(print_job_options->color.default_value.value(), true);
  EXPECT_THAT(print_job_options->color.allowed_values,
              ElementsAre(true, false));
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, Dpi) {
  DPIOption dpi;
  dpi.mutable_default_value()->set_horizontal(1000);
  dpi.mutable_default_value()->set_vertical(1500);
  std::vector<DPI> dpi_available_values(2);
  dpi_available_values[0].set_horizontal(1000);
  dpi_available_values[0].set_vertical(1500);
  dpi_available_values[1].set_horizontal(2000);
  dpi_available_values[1].set_vertical(2500);
  *dpi.mutable_allowed_values() = RepeatedPtrField<DPI>(
      dpi_available_values.begin(), dpi_available_values.end());
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_dpi() = dpi;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->dpi.default_value.has_value());
  EXPECT_EQ(print_job_options->dpi.default_value.value(),
            (Printer::Dpi{1000, 1500}));
  EXPECT_THAT(print_job_options->dpi.allowed_values,
              ElementsAre(Printer::Dpi{1000, 1500}, Printer::Dpi{2000, 2500}));
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, Quality) {
  QualityOption quality;
  quality.set_default_value(QualityType::QUALITY_DRAFT);
  quality.add_allowed_values(QualityType::QUALITY_DRAFT);
  quality.add_allowed_values(QualityType::QUALITY_NORMAL);
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_quality() = quality;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->quality.default_value.has_value());
  EXPECT_EQ(print_job_options->quality.default_value.value(),
            Printer::QualityType::kDraft);
  EXPECT_THAT(
      print_job_options->quality.allowed_values,
      ElementsAre(Printer::QualityType::kDraft, Printer::QualityType::kNormal));
}

TEST(ChromeOsPrintOptionsFromManagedPrintOptions, PrintAsImage) {
  BoolOption print_as_image;
  print_as_image.set_default_value(false);
  print_as_image.add_allowed_values(false);
  PrintJobOptions print_job_options_proto;
  *print_job_options_proto.mutable_print_as_image() = print_as_image;

  std::optional<Printer::ManagedPrintOptions> print_job_options =
      ChromeOsPrintOptionsFromManagedPrintOptions(print_job_options_proto);

  ASSERT_TRUE(print_job_options.has_value());
  ASSERT_TRUE(print_job_options->print_as_image.default_value.has_value());
  EXPECT_EQ(print_job_options->print_as_image.default_value.value(), false);
  EXPECT_THAT(print_job_options->print_as_image.allowed_values,
              ElementsAre(false));
}

}  // namespace

}  // namespace chromeos
