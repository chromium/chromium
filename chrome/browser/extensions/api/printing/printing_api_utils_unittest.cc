// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api_utils.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_features.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace idl = api::printing;

using testing::Contains;
using testing::Pair;

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";
constexpr char kDescription[] = "description";
constexpr char kUri[] = "ipp://192.168.1.5";
constexpr int kRank = 2;

constexpr int kCopies = 5;
constexpr int kHorizontalDpi = 300;
constexpr int kVerticalDpi = 400;
constexpr int kMediaSizeWidth = 210000;
constexpr int kMediaSizeHeight = 297000;
constexpr int kCustomMediaSizeMin = 2540;
constexpr char kMediaSizeVendorId[] = "iso_a4_210x297mm";
constexpr char kVendorItemId[] = "finishings";
constexpr char kVendorItemValue[] = "trim";
const printing::PaperMargins kPaperMarginsUm = {2960, 3150, 4130, 2830};

constexpr char kCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        },
        "media_size": {
          "width_microns": 210000,
          "height_microns": 297000,
          "vendor_id": "iso_a4_210x297mm"
        },
        "vendor_ticket_item": [
          {
            "id": "finishings",
            "value": "trim"
          }
        ],
        "collate": {
          "collate": false
        },
        "fit_to_page": {
          "type": "FIT"
        },
        "margins": {
          "top_microns": 2960,
          "right_microns": 3150,
          "bottom_microns": 4130,
          "left_microns": 2830
        },
      }
    })";

// Template for CJT with fit_to_page.
constexpr char kCjtWithFitToPageTemplate[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        },
        "media_size": {
          "width_microns": 210000,
          "height_microns": 297000,
          "vendor_id": "iso_a4_210x297mm"
        },
        "fit_to_page": {
          "type": "%s"
        },
        "collate": {
          "collate": false
        }
      }
    })";

constexpr char kInvalidVendorItemCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        },
        "media_size": {
          "width_microns": 210000,
          "height_microns": 297000,
          "vendor_id": "iso_a4_210x297mm"
        },
        "vendor_ticket_item": [
          {
            "id": "invalid-id",
            "value": "invalid-value"
          }
        ],
        "collate": {
          "collate": false
        }
      }
    })";

constexpr char kTemplateMargingsItemCjt[] = R"(
    {
      "version": "1.0",
      "print": {
          "color": {
            "type": "STANDARD_MONOCHROME"
          },
          "duplex": {
            "type": "NO_DUPLEX"
          },
          "page_orientation": {
            "type": "LANDSCAPE"
          },
          "copies": {
            "copies": 5
          },
          "dpi": {
            "horizontal_dpi": 300,
            "vertical_dpi": 400
          },
          "media_size": {
            "width_microns": 210000,
            "height_microns": 297000,
            "vendor_id": "iso_a4_210x297mm"
          },
          "vendor_ticket_item": [
            {
              "id": "finishings",
              "value": "trim"
            }
          ],
          "collate": {
            "collate": false
          },
          "margins": {
            "top_microns": %d,
            "right_microns": %d,
            "bottom_microns": %d,
            "left_microns": %d
          },
      }
    })";

constexpr char kIncompleteCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        }
      }
    })";

constexpr char kCjtNoFitToPageAndMargins[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        },
        "media_size": {
          "width_microns": 210000,
          "height_microns": 297000,
          "vendor_id": "iso_a4_210x297mm"
        },
        "vendor_ticket_item": [
          {
            "id": "finishings",
            "value": "trim"
          }
        ],
        "collate": {
          "collate": false
        }
      }
    })";

std::unique_ptr<printing::PrintSettings> ConstructPrintSettings() {
  auto settings = std::make_unique<printing::PrintSettings>();
  settings->set_color(printing::mojom::ColorModel::kColor);
  settings->set_duplex_mode(printing::mojom::DuplexMode::kLongEdge);
  settings->SetOrientation(/*landscape=*/true);
  settings->set_copies(kCopies);
  settings->set_dpi_xy(kHorizontalDpi, kVerticalDpi);
  printing::PrintSettings::RequestedMedia requested_media;
  requested_media.size_microns = gfx::Size(kMediaSizeWidth, kMediaSizeHeight);
  requested_media.vendor_id = kMediaSizeVendorId;
  settings->set_requested_media(requested_media);
  settings->set_collate(true);
  return settings;
}

printing::PrinterSemanticCapsAndDefaults ConstructPrinterCapabilities() {
  printing::PrinterSemanticCapsAndDefaults capabilities;
  capabilities.color_model = printing::mojom::ColorModel::kColor;
  capabilities.duplex_modes.push_back(printing::mojom::DuplexMode::kLongEdge);
  capabilities.copies_max = kCopies;
  capabilities.dpis.push_back(gfx::Size(kHorizontalDpi, kVerticalDpi));
  printing::PrinterSemanticCapsAndDefaults::Paper paper(
      /*display_name=*/"", kMediaSizeVendorId,
      gfx::Size(kMediaSizeWidth, kMediaSizeHeight));
  capabilities.papers.push_back(paper);
  capabilities.collate_capable = true;

  std::vector<printing::AdvancedCapabilityValue> media_source_values{
      {"auto", /*display_name=*/""},
      {"left", /*display_name=*/""},
      {"right", /*display_name=*/""}};
  printing::AdvancedCapability media_source(
      "media-source", /*display_name=*/"",
      printing::AdvancedCapability::Type::kString,
      /*default_value=*/"", std::move(media_source_values));
  capabilities.advanced_capabilities.emplace_back(std::move(media_source));

  return capabilities;
}

printing::PrinterSemanticCapsAndDefaults
ConstructPrinterCapabilitiesWithCustomSize() {
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  // Reset our papers and create a new paper with a custom size range.
  capabilities.papers.clear();
  printing::PrinterSemanticCapsAndDefaults::Paper paper(
      /*display_name=*/"", kMediaSizeVendorId,
      gfx::Size(kMediaSizeWidth, kCustomMediaSizeMin),
      /*printable_area_um=*/gfx::Rect(), kMediaSizeHeight,
      /*has_borderless_variant=*/false,
      /*supported_margins_um=*/kPaperMarginsUm);
  capabilities.papers.push_back(paper);

  return capabilities;
}

}  // namespace

TEST(PrintingApiUtilsTest, GetDefaultPrinterRules) {
  std::string default_printer_rules_str =
      R"({"kind": "local", "idPattern": "id.*", "namePattern": "name.*"})";
  std::optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(default_printer_rules_str);
  ASSERT_TRUE(default_printer_rules.has_value());
  EXPECT_EQ("local", default_printer_rules->kind);
  EXPECT_EQ("id.*", default_printer_rules->id_pattern);
  EXPECT_EQ("name.*", default_printer_rules->name_pattern);
}

TEST(PrintingApiUtilsTest, GetDefaultPrinterRules_EmptyPref) {
  std::string default_printer_rules_str;
  std::optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(default_printer_rules_str);
  EXPECT_FALSE(default_printer_rules.has_value());
}

TEST(PrintingApiUtilsTest, PrinterToIdl) {
  crosapi::mojom::LocalDestinationInfo printer(kId, kName, kDescription, true,
                                               kUri);

  std::optional<DefaultPrinterRules> default_printer_rules =
      DefaultPrinterRules();
  default_printer_rules->kind = "local";
  default_printer_rules->name_pattern = "n.*e";
  base::flat_map<std::string, int> recently_used_ranks = {{kId, kRank},
                                                          {"ok", 1}};
  idl::Printer idl_printer =
      PrinterToIdl(printer, default_printer_rules, recently_used_ranks);

  EXPECT_EQ(kId, idl_printer.id);
  EXPECT_EQ(kName, idl_printer.name);
  EXPECT_EQ(kDescription, idl_printer.description);
  EXPECT_EQ(kUri, idl_printer.uri);
  EXPECT_EQ(idl::PrinterSource::kPolicy, idl_printer.source);
  EXPECT_EQ(true, idl_printer.is_default);
  ASSERT_TRUE(idl_printer.recently_used_rank);
  EXPECT_EQ(kRank, *idl_printer.recently_used_rank);
}

TEST(PrintingApiUtilsTest, ParsePrintTicket) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        printing::features::kApiPrintingMarginsAndScale);
    base::Value::Dict cjt_ticket = base::test::ParseJsonDict(kCjt);
    std::unique_ptr<printing::PrintSettings> settings =
        ParsePrintTicket(std::move(cjt_ticket));

    ASSERT_TRUE(settings);
    EXPECT_EQ(printing::mojom::ColorModel::kGray, settings->color());
    EXPECT_EQ(printing::mojom::DuplexMode::kSimplex, settings->duplex_mode());
    EXPECT_TRUE(settings->landscape());
    EXPECT_EQ(5, settings->copies());
    EXPECT_EQ(gfx::Size(kHorizontalDpi, kVerticalDpi), settings->dpi_size());
    EXPECT_EQ(gfx::Size(kMediaSizeWidth, kMediaSizeHeight),
              settings->requested_media().size_microns);
    EXPECT_EQ(kMediaSizeVendorId, settings->requested_media().vendor_id);
    EXPECT_FALSE(settings->collate());
    EXPECT_THAT(settings->advanced_settings(),
                Contains(Pair(kVendorItemId, kVendorItemValue)));
    // Since the feature is disabled, no print-scaling should be applied and the
    // margin type should be the default.
    EXPECT_EQ(printing::mojom::PrintScalingType::kUnknownPrintScalingType,
              settings->print_scaling());
    EXPECT_EQ(settings->margin_type(),
              printing::mojom::MarginType::kDefaultMargins);
    EXPECT_EQ(settings->requested_custom_margins_in_microns(),
              printing::PageMargins());
  }

  // Once the feature is enabled, print-scaling should be applied and the margin
  // type should be custom.
  {
    base::test::ScopedFeatureList feature_list(
        printing::features::kApiPrintingMarginsAndScale);
    std::unique_ptr<printing::PrintSettings> settings =
        ParsePrintTicket(base::test::ParseJsonDict(kCjt));
    ASSERT_TRUE(settings);
    EXPECT_EQ(printing::mojom::PrintScalingType::kFit,
              settings->print_scaling());

    EXPECT_EQ(settings->margin_type(),
              printing::mojom::MarginType::kCustomMargins);
    const printing::PageMargins kExpectedPageMargins = {
        /*header=*/0,
        /*footer=*/0,
        static_cast<int>(kPaperMarginsUm.left_margin_um),
        static_cast<int>(kPaperMarginsUm.right_margin_um),
        static_cast<int>(kPaperMarginsUm.top_margin_um),
        static_cast<int>(kPaperMarginsUm.bottom_margin_um)};
    EXPECT_EQ(settings->requested_custom_margins_in_microns(),
              kExpectedPageMargins);
  }
}

// Test that parsing CJT with FitToPage values either succeeds or fails
// if the value is unknown.
TEST(PrintingApiUtilsTest, ParsePrintTicketFitToPage) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  struct test_case {
    std::string_view fit_to_page_type;
    printing::mojom::PrintScalingType expected_print_scaling;
  } constexpr kTestCases[] = {
      {"AUTO", printing::mojom::PrintScalingType::kAuto},
      {"AUTO_FIT", printing::mojom::PrintScalingType::kAutoFit},
      {"FIT", printing::mojom::PrintScalingType::kFit},
      {"FILL", printing::mojom::PrintScalingType::kFill},
      {"NONE", printing::mojom::PrintScalingType::kNone},
      {"random-value",
       printing::mojom::PrintScalingType::kUnknownPrintScalingType},
  };

  for (const auto& test_case : kTestCases) {
    const std::string cjt_json =
        absl::StrFormat(kCjtWithFitToPageTemplate, test_case.fit_to_page_type);
    base::Value::Dict cjt_ticket = base::test::ParseJsonDict(cjt_json);
    std::unique_ptr<printing::PrintSettings> settings =
        ParsePrintTicket(std::move(cjt_ticket));
    ASSERT_TRUE(settings);
    EXPECT_EQ(test_case.expected_print_scaling, settings->print_scaling());
  }
}

TEST(PrintingApiUtilsTest, ParsePrintTicketNoFitToPageAndNoMargins) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  base::Value::Dict cjt_ticket =
      base::test::ParseJsonDict(kCjtNoFitToPageAndMargins);
  std::unique_ptr<printing::PrintSettings> settings =
      ParsePrintTicket(std::move(cjt_ticket));

  ASSERT_TRUE(settings);
  // When print-scaling values are missing, no scaling should be applied.
  EXPECT_EQ(printing::mojom::PrintScalingType::kUnknownPrintScalingType,
            settings->print_scaling());
  ASSERT_TRUE(settings);
  // When margins are missing, default margins should be used
  EXPECT_EQ(settings->margin_type(),
            printing::mojom::MarginType::kDefaultMargins);
  EXPECT_FALSE(settings->borderless());
}

TEST(PrintingApiUtilsTest, ParsePrintTicketInvalidVendorItem) {
  // Even though this CJT has an invalid vendor item, it should parse correctly.
  // It will fail when the CJT is checked vs the printer capabilities.
  EXPECT_TRUE(
      ParsePrintTicket(base::test::ParseJsonDict(kInvalidVendorItemCjt)));
}

TEST(PrintingApiUtilsTest, ParsePrintTicketInvalidMarginsItem) {
  const std::string kInvalidMarginsCjt =
      base::StringPrintf(kTemplateMargingsItemCjt, -40, 20, -30, 40);
  EXPECT_TRUE(ParsePrintTicket(base::test::ParseJsonDict(kInvalidMarginsCjt)));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      printing::features::kApiPrintingMarginsAndScale);
  EXPECT_FALSE(ParsePrintTicket(base::test::ParseJsonDict(kInvalidMarginsCjt)));
}

TEST(PrintingApiUtilsTest, ParsePrintTicket_IncompleteCjt) {
  EXPECT_FALSE(ParsePrintTicket(base::test::ParseJsonDict(kIncompleteCjt)));
}

TEST(PrintingApiUtilsTest, ParsePrintTicket_BorderlessMargins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      printing::features::kApiPrintingMarginsAndScale);

  const std::string kBorderlessCjt =
      base::StringPrintf(kTemplateMargingsItemCjt, 0, 0, 0, 0);
  std::unique_ptr<printing::PrintSettings> settings =
      ParsePrintTicket(base::test::ParseJsonDict(kBorderlessCjt));

  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->margin_type(), printing::mojom::MarginType::kNoMargins);
  EXPECT_TRUE(settings->borderless());

  const printing::PageMargins kExpectedPageMargins = {/*header=*/0,
                                                      /*footer=*/0,
                                                      /*left=*/0,
                                                      /*right=*/0,
                                                      /*top=*/0,
                                                      /*bottom=*/0};
  EXPECT_EQ(settings->requested_custom_margins_in_microns(),
            kExpectedPageMargins);
}

TEST(PrintingApiUtilsTest, ParsePrintTicket_MixedMargins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      printing::features::kApiPrintingMarginsAndScale);

  const std::string kMixedMarginsCjt =
      base::StringPrintf(kTemplateMargingsItemCjt, 0, 3150, 0, 2830);
  std::unique_ptr<printing::PrintSettings> settings =
      ParsePrintTicket(base::test::ParseJsonDict(kMixedMarginsCjt));

  ASSERT_TRUE(settings);
  // With mixed margins (some 0, some non-0), we should get custom margins but
  // not borderless
  EXPECT_EQ(settings->margin_type(),
            printing::mojom::MarginType::kCustomMargins);
  EXPECT_FALSE(settings->borderless());

  const printing::PageMargins kExpectedPageMargins = {/*header=*/0,
                                                      /*footer=*/0,
                                                      /*left=*/2830,
                                                      /*right=*/3150,
                                                      /*top=*/0,
                                                      /*bottom=*/0};
  EXPECT_EQ(settings->requested_custom_margins_in_microns(),
            kExpectedPageMargins);
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  EXPECT_TRUE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Color) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.color_model = printing::mojom::ColorModel::kUnknownColorModel;
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Duplex) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.duplex_modes = {printing::mojom::DuplexMode::kSimplex};
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Copies) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.copies_max = 1;
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Dpi) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.dpis = {gfx::Size(100, 100)};
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibility_MediaSize) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.papers.clear();
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibilityCustomMediaSize) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilitiesWithCustomSize();
  EXPECT_TRUE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibilityCustomMediaSizeLongWidth) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  // Update the requested media so the width is wider than our custom size.
  printing::PrintSettings::RequestedMedia media = settings->requested_media();
  media.size_microns.set_width(kMediaSizeWidth + 1);
  settings->set_requested_media(media);
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilitiesWithCustomSize();
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibilityCustomMediaSizeShortHeight) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  // Update the requested media so the length is shorter than our custom size.
  printing::PrintSettings::RequestedMedia media = settings->requested_media();
  media.size_microns.set_height(kCustomMediaSizeMin - 1);
  settings->set_requested_media(media);
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilitiesWithCustomSize();
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Collate) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.collate_capable = false;
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Advanced) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  settings->advanced_settings().emplace("finishings", "trim");
  settings->advanced_settings().emplace("media-source", "right");
  EXPECT_TRUE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibility_AdvancedBadAttribute) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  settings->advanced_settings().emplace("unsupported-name", "trim");
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibility_AdvancedBadValue) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  settings->advanced_settings().emplace("media-source", "unsupported-value");
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibility_AdvancedInvalidValue) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  settings->advanced_settings().emplace("finishings", 123);
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibility_PrintScaling) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.print_scaling_types.clear();

  const std::vector<printing::mojom::PrintScalingType> kScalingTypes = {
      printing::mojom::PrintScalingType::kUnknownPrintScalingType,
      printing::mojom::PrintScalingType::kAuto,
      printing::mojom::PrintScalingType::kAutoFit,
      printing::mojom::PrintScalingType::kFit,
      printing::mojom::PrintScalingType::kFill,
      printing::mojom::PrintScalingType::kNone,
  };

  // Test with feature disabled - all types should pass as check is skipped.
  for (const auto& scaling_type : kScalingTypes) {
    settings->set_print_scaling(scaling_type);
    EXPECT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Re-enable feature for further tests.
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  capabilities.print_scaling_types.clear();
  // Capabilities have no print scaling types, so all types except unknown
  // should fail.
  for (const auto& scaling_type : kScalingTypes) {
    settings->set_print_scaling(scaling_type);
    if (scaling_type ==
        printing::mojom::PrintScalingType::kUnknownPrintScalingType) {
      EXPECT_TRUE(
          CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    } else {
      EXPECT_FALSE(
          CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    }
  }

  // Add all scaling types to capabilities
  capabilities.print_scaling_types = kScalingTypes;

  // Now all types should pass
  for (const auto& scaling_type : kScalingTypes) {
    settings->set_print_scaling(scaling_type);
    EXPECT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Test selective support - only kFit and kAuto are supported
  capabilities.print_scaling_types = {printing::mojom::PrintScalingType::kFit,
                                      printing::mojom::PrintScalingType::kAuto};

  // Test each scaling type against the selective support
  for (const auto& scaling_type : kScalingTypes) {
    settings->set_print_scaling(scaling_type);
    // Unknown type and supported types should pass.
    if (scaling_type == printing::mojom::PrintScalingType::kFit ||
        scaling_type == printing::mojom::PrintScalingType::kAuto ||
        scaling_type ==
            printing::mojom::PrintScalingType::kUnknownPrintScalingType) {
      EXPECT_TRUE(
          CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    } else {
      // Other types should fail as they are not supported.
      EXPECT_FALSE(
          CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    }
  }
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Margins) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();

  const printing::PageMargins kMargins = {
      /*header=*/0,  /*footer=*/0,
      /*left=*/1500, /*right=*/500,
      /*top=*/3530,  /*bottom=*/5525};
  settings->SetCustomMargins(kMargins);
  settings->set_margin_type(printing::mojom::MarginType::kCustomMargins);

  // There must be no supported margins for the check to pass.
  for (const auto& paper : capabilities.papers) {
    ASSERT_FALSE(paper.supported_margins_um().has_value());
  }

  {
    // Test with feature disabled - despite margins not being supported and
    // provided, the check should pass as the feature is disabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        printing::features::kApiPrintingMarginsAndScale);
    EXPECT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Re-enable feature for further tests.
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  // Add a paper with supported margins.
  const printing::PaperMargins kSupportedMargins(1500, 500, 3241, 3451);
  capabilities.papers.emplace_back(
      /*display_name=*/"", /*vendor_id=*/"",
      /*size_um=*/gfx::Size(kMediaSizeWidth, kMediaSizeHeight),
      /*printable_area_um=*/gfx::Rect(kMediaSizeWidth, kMediaSizeHeight),
      /*max_height_um=*/0, /*has_borderless_variant=*/false,
      /*supported_margins_um=*/kSupportedMargins);

  // Default margins should be supported.
  {
    settings->SetCustomMargins(printing::PageMargins());
    settings->set_margin_type(printing::mojom::MarginType::kDefaultMargins);
    EXPECT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Set supported margins and settings to different values. The check should
  // fail.
  {
    settings->SetCustomMargins(kMargins);
    settings->set_margin_type(printing::mojom::MarginType::kCustomMargins);
    EXPECT_FALSE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Now, update the settings to have supported margins. The check should pass.
  {
    settings->SetCustomMargins(printing::PageMargins(
        /*header=*/0, /*footer=*/0,
        /*left=*/kSupportedMargins.left_margin_um,
        /*right=*/kSupportedMargins.right_margin_um,
        /*top=*/kSupportedMargins.top_margin_um,
        /*bottom=*/kSupportedMargins.bottom_margin_um));
    EXPECT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Test borderless variant.
  {
    settings->SetCustomMargins(printing::PageMargins());
    settings->set_margin_type(printing::mojom::MarginType::kNoMargins);
    settings->set_borderless(true);
    EXPECT_FALSE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    capabilities.papers.emplace_back(
        printing::PrinterSemanticCapsAndDefaults::Paper(
            /*display_name=*/"", /*vendor_id=*/"",
            /*size_um=*/gfx::Size(kMediaSizeWidth, kMediaSizeHeight),
            /*printable_area_um=*/gfx::Rect(kMediaSizeWidth, kMediaSizeHeight),
            /*max_height_um=*/0, /*has_borderless_variant=*/true,
            /*supported_margins_um=*/kSupportedMargins));
    EXPECT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }

  // Invalid case: try to set borderless in settings, but still provide margin
  // values. This must be invalid.
  {
    settings->set_borderless(true);
    settings->SetCustomMargins(kMargins);
    settings->set_margin_type(printing::mojom::MarginType::kCustomMargins);
    EXPECT_FALSE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
  }
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilities_PrintScalingHistogram) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  // Set supported print scaling types.
  capabilities.print_scaling_types = {printing::mojom::PrintScalingType::kFit,
                                      printing::mojom::PrintScalingType::kAuto};

  // Try with the feature disabled first - no matter if the print scaling is
  // correct or not, the histogram should not be recorded.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        printing::features::kApiPrintingMarginsAndScale);

    // Unsupported print scaling.
    settings->set_print_scaling(printing::mojom::PrintScalingType::kFill);
    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));

    // Supported print scaling.
    settings->set_print_scaling(printing::mojom::PrintScalingType::kFit);
    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectTotalCount(
        "Extensions.Printing.UsesSupportedPrintScaling", 0);
  }

  // Enable the feature now.
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  // Verify that the UMA histogram is recorded with false when unsupported
  // print scaling is passed.
  {
    base::HistogramTester histogram_tester;
    settings->set_print_scaling(printing::mojom::PrintScalingType::kAutoFit);
    EXPECT_FALSE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectUniqueSample(
        "Extensions.Printing.UsesSupportedPrintScaling", false, 1);
  }

  // Verify that the UMA histogram is recorded with true when supported
  // print scaling is passed.
  {
    base::HistogramTester histogram_tester;
    settings->set_print_scaling(printing::mojom::PrintScalingType::kAuto);
    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectUniqueSample(
        "Extensions.Printing.UsesSupportedPrintScaling", true, 1);
  }

  // If unknown print scaling is set, the histogram mustn't be recorded.
  {
    base::HistogramTester histogram_tester;
    settings->set_print_scaling(
        printing::mojom::PrintScalingType::kUnknownPrintScalingType);
    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectTotalCount(
        "Extensions.Printing.UsesSupportedPrintScaling", 0);
  }
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilities_MarginHistogram) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();

  const printing::PaperMargins kSupportedMargins(1500, 500, 3241, 3451);
  const printing::PageMargins kMargins = {
      /*header=*/0,
      /*footer=*/0,
      /*left=*/kSupportedMargins.left_margin_um,
      /*right=*/kSupportedMargins.right_margin_um,
      /*top=*/kSupportedMargins.top_margin_um,
      /*bottom=*/kSupportedMargins.bottom_margin_um};
  settings->SetCustomMargins(kMargins);
  settings->set_margin_type(printing::mojom::MarginType::kCustomMargins);

  // Try with the feature disabled first - no matter if the margins are correct
  // or not, the histogram should not be recorded.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        printing::features::kApiPrintingMarginsAndScale);

    printing::PrinterSemanticCapsAndDefaults capabilities =
        ConstructPrinterCapabilities();
    // There must be no supported margins first.
    for (const auto& paper : capabilities.papers) {
      ASSERT_FALSE(paper.supported_margins_um().has_value());
    }

    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));

    // Add a paper with supported margins.
    capabilities.papers.emplace_back(
        /*display_name=*/"", /*vendor_id=*/"",
        /*size_um=*/gfx::Size(kMediaSizeWidth, kMediaSizeHeight),
        /*printable_area_um=*/gfx::Rect(kMediaSizeWidth, kMediaSizeHeight),
        /*max_height_um=*/0, /*has_borderless_variant=*/false,
        /*supported_margins_um=*/kSupportedMargins);

    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectTotalCount(
        "Extensions.Printing.UsesSupportedMargins", 0);
  }

  // Enable the feature now. The histogram should be recorded.
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);
  // Test with margins that are not supported.
  {
    base::HistogramTester histogram_tester;
    printing::PrinterSemanticCapsAndDefaults capabilities =
        ConstructPrinterCapabilities();
    // There must be no supported margins first.
    for (const auto& paper : capabilities.papers) {
      ASSERT_FALSE(paper.supported_margins_um().has_value());
    }

    // Verify that the UMA histogram is recorded with false when unsupported
    // margins are passed.
    EXPECT_FALSE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectUniqueSample(
        "Extensions.Printing.UsesSupportedMargins", false, 1);
  }

  // Test with margins that are supported.
  {
    base::HistogramTester histogram_tester;
    printing::PrinterSemanticCapsAndDefaults capabilities =
        ConstructPrinterCapabilities();
    // Add a paper with supported margins.
    capabilities.papers.emplace_back(
        /*display_name=*/"", /*vendor_id=*/"",
        /*size_um=*/gfx::Size(kMediaSizeWidth, kMediaSizeHeight),
        /*printable_area_um=*/gfx::Rect(kMediaSizeWidth, kMediaSizeHeight),
        /*max_height_um=*/0, /*has_borderless_variant=*/false,
        /*supported_margins_um=*/kSupportedMargins);

    // Verify that the UMA histogram is recorded with true when margins match
    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectUniqueSample(
        "Extensions.Printing.UsesSupportedMargins", true, 1);

    // If default margins are set, the histogram mustn't be recorded.
    settings->SetCustomMargins(printing::PageMargins());
    settings->set_margin_type(printing::mojom::MarginType::kDefaultMargins);
    ASSERT_TRUE(
        CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
    histogram_tester.ExpectUniqueSample(
        "Extensions.Printing.UsesSupportedMargins", true, 1);
  }
}

}  // namespace extensions
