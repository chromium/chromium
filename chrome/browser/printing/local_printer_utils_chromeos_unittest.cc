// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/local_printer_utils_chromeos.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace printing {

TEST(ManagedPrintOptionsToMojom, MediaSize) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<chromeos::Printer::Size> media_size;
  media_size.default_value = chromeos::Printer::Size{.width = 30, .height = 60};
  media_size.allowed_values = std::vector<chromeos::Printer::Size>{
      {.width = 30, .height = 60}, {.width = 45, .height = 90}};
  managed_print_options.media_size = media_size;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  ASSERT_TRUE(mojom_managed_print_options->media_size->default_value);
  EXPECT_EQ(*mojom_managed_print_options->media_size->default_value,
            crosapi::mojom::Size(30u, 60u));
  ASSERT_TRUE(mojom_managed_print_options->media_size->allowed_values);
  ASSERT_EQ(mojom_managed_print_options->media_size->allowed_values->size(),
            2u);
  EXPECT_EQ(*mojom_managed_print_options->media_size->allowed_values.value()[0],
            crosapi::mojom::Size(30u, 60u));
  EXPECT_EQ(*mojom_managed_print_options->media_size->allowed_values.value()[1],
            crosapi::mojom::Size(45u, 90u));
}

TEST(ManagedPrintOptionsToMojom, MediaType) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<std::string> media_type;
  media_type.default_value = "paper";
  media_type.allowed_values = {"paper", "metal", "wood"};
  managed_print_options.media_type = media_type;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  EXPECT_EQ(mojom_managed_print_options->media_type->default_value, "paper");
  ASSERT_TRUE(mojom_managed_print_options->media_type->allowed_values);
  EXPECT_THAT(*mojom_managed_print_options->media_type->allowed_values,
              ElementsAre("paper", "metal", "wood"));
}

TEST(ManagedPrintOptionsToMojom, DuplexOption) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<chromeos::Printer::DuplexType> duplex_option;
  duplex_option.default_value = chromeos::Printer::DuplexType::kOneSided;
  duplex_option.allowed_values = {chromeos::Printer::DuplexType::kOneSided,
                                  chromeos::Printer::DuplexType::kShortEdge,
                                  chromeos::Printer::DuplexType::kLongEdge};
  managed_print_options.duplex = duplex_option;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  EXPECT_EQ(mojom_managed_print_options->duplex->default_value,
            crosapi::mojom::DuplexType::kOneSided);
  ASSERT_TRUE(mojom_managed_print_options->duplex->allowed_values);
  EXPECT_THAT(*mojom_managed_print_options->duplex->allowed_values,
              ElementsAre(crosapi::mojom::DuplexType::kOneSided,
                          crosapi::mojom::DuplexType::kShortEdge,
                          crosapi::mojom::DuplexType::kLongEdge));
}

TEST(ManagedPrintOptionsToMojom, Color) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<bool> color_option;
  color_option.default_value = false;
  color_option.allowed_values = {false, true};
  managed_print_options.color = color_option;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  EXPECT_EQ(mojom_managed_print_options->color->default_value, false);
  ASSERT_TRUE(mojom_managed_print_options->color->allowed_values);
  EXPECT_THAT(*mojom_managed_print_options->color->allowed_values,
              ElementsAre(false, true));
}

TEST(ManagedPrintOptionsToMojom, Dpi) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<chromeos::Printer::Dpi> dpi;
  dpi.default_value =
      chromeos::Printer::Dpi{.horizontal = 1000, .vertical = 1500};
  dpi.allowed_values = std::vector<chromeos::Printer::Dpi>{
      {.horizontal = 1000, .vertical = 1500},
      {.horizontal = 1500, .vertical = 3000}};
  managed_print_options.dpi = dpi;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  ASSERT_TRUE(mojom_managed_print_options->dpi->default_value);
  EXPECT_EQ(*mojom_managed_print_options->dpi->default_value,
            crosapi::mojom::Dpi(1000u, 1500u));
  ASSERT_TRUE(mojom_managed_print_options->dpi->allowed_values);
  ASSERT_EQ(mojom_managed_print_options->dpi->allowed_values->size(), 2u);
  EXPECT_EQ(*mojom_managed_print_options->dpi->allowed_values.value()[0],
            crosapi::mojom::Dpi(1000u, 1500u));
  EXPECT_EQ(*mojom_managed_print_options->dpi->allowed_values.value()[1],
            crosapi::mojom::Dpi(1500u, 3000u));
}

TEST(ManagedPrintOptionsToMojom, Quality) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<chromeos::Printer::QualityType> quality_option;
  quality_option.default_value = chromeos::Printer::QualityType::kDraft;
  quality_option.allowed_values = {chromeos::Printer::QualityType::kDraft,
                                   chromeos::Printer::QualityType::kNormal,
                                   chromeos::Printer::QualityType::kHigh};
  managed_print_options.quality = quality_option;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  EXPECT_EQ(mojom_managed_print_options->quality->default_value,
            crosapi::mojom::QualityType::kDraft);
  ASSERT_TRUE(mojom_managed_print_options->quality->allowed_values);
  EXPECT_THAT(*mojom_managed_print_options->quality->allowed_values,
              ElementsAre(crosapi::mojom::QualityType::kDraft,
                          crosapi::mojom::QualityType::kNormal,
                          crosapi::mojom::QualityType::kHigh));
}

TEST(ManagedPrintOptionsToMojom, PrintAsImage) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;

  chromeos::Printer::PrintOption<bool> print_as_image;
  print_as_image.default_value = false;
  print_as_image.allowed_values = {true, false};
  managed_print_options.print_as_image = print_as_image;

  const auto mojom_managed_print_options =
      ManagedPrintOptionsToMojom(managed_print_options);
  EXPECT_EQ(mojom_managed_print_options->print_as_image->default_value, false);
  ASSERT_TRUE(mojom_managed_print_options->print_as_image->allowed_values);
  EXPECT_THAT(*mojom_managed_print_options->print_as_image->allowed_values,
              ElementsAre(true, false));
}

}  // namespace printing
