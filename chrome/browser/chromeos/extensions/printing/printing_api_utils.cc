// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "third_party/re2/src/re2/re2.h"

namespace extensions {

namespace idl = api::printing;

namespace {

constexpr char kLocal[] = "local";
constexpr char kKind[] = "kind";
constexpr char kIdPattern[] = "idPattern";
constexpr char kNamePattern[] = "namePattern";

idl::PrinterSource PrinterSourceToIdl(chromeos::Printer::Source source) {
  switch (source) {
    case chromeos::Printer::Source::SRC_USER_PREFS:
      return idl::PRINTER_SOURCE_USER;
    case chromeos::Printer::Source::SRC_POLICY:
      return idl::PRINTER_SOURCE_POLICY;
  }
  NOTREACHED();
  return idl::PRINTER_SOURCE_USER;
}

bool DoesPrinterMatchDefaultPrinterRules(
    const chromeos::Printer& printer,
    const base::Optional<DefaultPrinterRules>& rules) {
  if (!rules.has_value())
    return false;
  return (rules->kind.empty() || rules->kind == kLocal) &&
         (rules->id_pattern.empty() ||
          RE2::FullMatch(printer.id(), rules->id_pattern)) &&
         (rules->name_pattern.empty() ||
          RE2::FullMatch(printer.display_name(), rules->name_pattern));
}

}  // namespace

base::Optional<DefaultPrinterRules> GetDefaultPrinterRules(
    const std::string& default_destination_selection_rules) {
  if (default_destination_selection_rules.empty())
    return base::nullopt;

  base::Optional<base::Value> default_destination_selection_rules_value =
      base::JSONReader::Read(default_destination_selection_rules);
  if (!default_destination_selection_rules_value)
    return base::nullopt;

  DefaultPrinterRules default_printer_rules;
  const std::string* kind =
      default_destination_selection_rules_value->FindStringKey(kKind);
  if (kind)
    default_printer_rules.kind = *kind;
  const std::string* id_pattern =
      default_destination_selection_rules_value->FindStringKey(kIdPattern);
  if (id_pattern)
    default_printer_rules.id_pattern = *id_pattern;
  const std::string* name_pattern =
      default_destination_selection_rules_value->FindStringKey(kNamePattern);
  if (name_pattern)
    default_printer_rules.name_pattern = *name_pattern;
  return default_printer_rules;
}

idl::Printer PrinterToIdl(
    const chromeos::Printer& printer,
    const base::Optional<DefaultPrinterRules>& default_printer_rules,
    const base::flat_map<std::string, int>& recently_used_ranks) {
  idl::Printer idl_printer;
  idl_printer.id = printer.id();
  idl_printer.name = printer.display_name();
  idl_printer.description = printer.description();
  idl_printer.uri = printer.uri().GetNormalized(true /*always_print_port*/);
  idl_printer.source = PrinterSourceToIdl(printer.source());
  idl_printer.is_default =
      DoesPrinterMatchDefaultPrinterRules(printer, default_printer_rules);
  auto it = recently_used_ranks.find(printer.id());
  if (it != recently_used_ranks.end())
    idl_printer.recently_used_rank = std::make_unique<int>(it->second);
  else
    idl_printer.recently_used_rank = nullptr;
  return idl_printer;
}

idl::PrinterStatus PrinterStatusToIdl(chromeos::PrinterErrorCode status) {
  switch (status) {
    case chromeos::PrinterErrorCode::NO_ERROR:
      return idl::PRINTER_STATUS_AVAILABLE;
    case chromeos::PrinterErrorCode::PAPER_JAM:
      return idl::PRINTER_STATUS_PAPER_JAM;
    case chromeos::PrinterErrorCode::OUT_OF_PAPER:
      return idl::PRINTER_STATUS_OUT_OF_PAPER;
    case chromeos::PrinterErrorCode::OUT_OF_INK:
      return idl::PRINTER_STATUS_OUT_OF_INK;
    case chromeos::PrinterErrorCode::DOOR_OPEN:
      return idl::PRINTER_STATUS_DOOR_OPEN;
    case chromeos::PrinterErrorCode::PRINTER_UNREACHABLE:
      return idl::PRINTER_STATUS_UNREACHABLE;
    case chromeos::PrinterErrorCode::TRAY_MISSING:
      return idl::PRINTER_STATUS_TRAY_MISSING;
    case chromeos::PrinterErrorCode::OUTPUT_FULL:
      return idl::PRINTER_STATUS_OUTPUT_FULL;
    case chromeos::PrinterErrorCode::STOPPED:
      return idl::PRINTER_STATUS_STOPPED;
    default:
      return idl::PRINTER_STATUS_GENERIC_ISSUE;
  }
  return idl::PRINTER_STATUS_GENERIC_ISSUE;
}

std::unique_ptr<printing::PrintSettings> ParsePrintTicket(base::Value ticket) {
  cloud_devices::CloudDeviceDescription description;
  if (!description.InitFromValue(std::move(ticket)))
    return nullptr;

  auto settings = std::make_unique<printing::PrintSettings>();

  cloud_devices::printer::ColorTicketItem color;
  if (!color.LoadFrom(description))
    return nullptr;
  switch (color.value().type) {
    case cloud_devices::printer::ColorType::STANDARD_MONOCHROME:
    case cloud_devices::printer::ColorType::CUSTOM_MONOCHROME:
      settings->set_color(printing::mojom::ColorModel::kGray);
      break;

    case cloud_devices::printer::ColorType::STANDARD_COLOR:
    case cloud_devices::printer::ColorType::CUSTOM_COLOR:
    case cloud_devices::printer::ColorType::AUTO_COLOR:
      settings->set_color(printing::mojom::ColorModel::kColor);
      break;

    default:
      NOTREACHED();
      break;
  }

  cloud_devices::printer::DuplexTicketItem duplex;
  if (!duplex.LoadFrom(description))
    return nullptr;
  switch (duplex.value()) {
    case cloud_devices::printer::DuplexType::NO_DUPLEX:
      settings->set_duplex_mode(printing::mojom::DuplexMode::kSimplex);
      break;
    case cloud_devices::printer::DuplexType::LONG_EDGE:
      settings->set_duplex_mode(printing::mojom::DuplexMode::kLongEdge);
      break;
    case cloud_devices::printer::DuplexType::SHORT_EDGE:
      settings->set_duplex_mode(printing::mojom::DuplexMode::kShortEdge);
      break;
    default:
      NOTREACHED();
      break;
  }

  cloud_devices::printer::OrientationTicketItem orientation;
  if (!orientation.LoadFrom(description))
    return nullptr;
  switch (orientation.value()) {
    case cloud_devices::printer::OrientationType::LANDSCAPE:
      settings->SetOrientation(/*landscape=*/true);
      break;
    case cloud_devices::printer::OrientationType::PORTRAIT:
      settings->SetOrientation(/*landscape=*/false);
      break;
    default:
      NOTREACHED();
      break;
  }

  cloud_devices::printer::CopiesTicketItem copies;
  if (!copies.LoadFrom(description) || copies.value() < 1)
    return nullptr;
  settings->set_copies(copies.value());

  cloud_devices::printer::DpiTicketItem dpi;
  if (!dpi.LoadFrom(description))
    return nullptr;
  settings->set_dpi_xy(dpi.value().horizontal, dpi.value().vertical);

  cloud_devices::printer::MediaTicketItem media;
  if (!media.LoadFrom(description))
    return nullptr;
  cloud_devices::printer::Media media_value = media.value();
  printing::PrintSettings::RequestedMedia requested_media;
  if (media_value.width_um <= 0 || media_value.height_um <= 0 ||
      media_value.vendor_id.empty()) {
    return nullptr;
  }
  requested_media.size_microns =
      gfx::Size(media_value.width_um, media_value.height_um);
  requested_media.vendor_id = media_value.vendor_id;
  settings->set_requested_media(requested_media);

  cloud_devices::printer::CollateTicketItem collate;
  if (!collate.LoadFrom(description))
    return nullptr;
  settings->set_collate(collate.value());

  return settings;
}

bool CheckSettingsAndCapabilitiesCompatibility(
    const printing::PrintSettings& settings,
    const printing::PrinterSemanticCapsAndDefaults& capabilities) {
  if (settings.collate() && !capabilities.collate_capable)
    return false;

  if (settings.copies() > capabilities.copies_max)
    return false;

  if (!base::Contains(capabilities.duplex_modes, settings.duplex_mode()))
    return false;

  base::Optional<bool> is_color =
      ::printing::IsColorModelSelected(settings.color());
  bool color_mode_selected = is_color.has_value() && is_color.value();
  if (!color_mode_selected &&
      capabilities.bw_model ==
          printing::mojom::ColorModel::kUnknownColorModel) {
    return false;
  }
  if (color_mode_selected &&
      capabilities.color_model ==
          printing::mojom::ColorModel::kUnknownColorModel) {
    return false;
  }

  if (!base::Contains(capabilities.dpis, settings.dpi_size()))
    return false;

  const printing::PrintSettings::RequestedMedia& requested_media =
      settings.requested_media();
  return std::any_of(
      capabilities.papers.begin(), capabilities.papers.end(),
      [&requested_media](
          const printing::PrinterSemanticCapsAndDefaults::Paper& paper) {
        return paper.size_um == requested_media.size_microns &&
               paper.vendor_id == requested_media.vendor_id;
      });
}

}  // namespace extensions
