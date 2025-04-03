// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api_utils.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_features.h"
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
        }
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

constexpr char kCjtNoFitToPage[] = R"(
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
      /*printable_area_um=*/gfx::Rect(), kMediaSizeHeight);
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
  // When feature is disabled, no print-scaling should be applied.
  EXPECT_EQ(printing::mojom::PrintScalingType::kUnknownPrintScalingType,
            settings->print_scaling());

  // When feature is enabled, print-scaling should be applied.
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);
  settings = ParsePrintTicket(base::test::ParseJsonDict(kCjt));
  ASSERT_TRUE(settings);
  EXPECT_EQ(printing::mojom::PrintScalingType::kFit, settings->print_scaling());
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

TEST(PrintingApiUtilsTest, ParsePrintTicketNoFitToPage) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  base::Value::Dict cjt_ticket = base::test::ParseJsonDict(kCjtNoFitToPage);
  std::unique_ptr<printing::PrintSettings> settings =
      ParsePrintTicket(std::move(cjt_ticket));

  ASSERT_TRUE(settings);
  // When print-scaling values are missing, no scaling should be applied.
  EXPECT_EQ(printing::mojom::PrintScalingType::kUnknownPrintScalingType,
            settings->print_scaling());
}

TEST(PrintingApiUtilsTest, ParsePrintTicketInvalidVendorItem) {
  // Even though this CJT has an invalid vendor item, it should parse correctly.
  // It will fail when the CJT is checked vs the printer capabilities.
  base::Value::Dict cjt_ticket =
      base::test::ParseJsonDict(kInvalidVendorItemCjt);
  EXPECT_TRUE(ParsePrintTicket(std::move(cjt_ticket)));
}

TEST(PrintingApiUtilsTest, ParsePrintTicket_IncompleteCjt) {
  base::Value::Dict incomplete_cjt_ticket =
      base::test::ParseJsonDict(kIncompleteCjt);
  EXPECT_FALSE(ParsePrintTicket(std::move(incomplete_cjt_ticket)));
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

}  // namespace extensions
