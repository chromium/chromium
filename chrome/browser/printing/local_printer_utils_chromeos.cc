// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/local_printer_utils_chromeos.h"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"

namespace printing {

namespace {

template <typename MojoOptionType,
          typename MojoOptionValueType,
          typename ChromeOsOptionValueType>
mojo::StructPtr<MojoOptionType> PrintOptionToMojom(
    const chromeos::Printer::PrintOption<ChromeOsOptionValueType>& print_option,
    base::RepeatingCallback<MojoOptionValueType(const ChromeOsOptionValueType&)>
        get_mojo_option_value) {
  auto result = MojoOptionType::New();

  if (print_option.default_value.has_value()) {
    result->default_value =
        get_mojo_option_value.Run(print_option.default_value.value());
  }

  if (!print_option.allowed_values.empty()) {
    result->allowed_values = std::vector<MojoOptionValueType>();
    result->allowed_values->reserve(print_option.allowed_values.size());
    for (const auto& allowed_value : print_option.allowed_values) {
      result->allowed_values->push_back(
          get_mojo_option_value.Run(allowed_value));
    }
  }

  return result;
}

template <typename MojoOptionType,
          typename MojoOptionValueType = std::remove_reference_t<
              decltype(MojoOptionType().default_value.value())>,
          typename ChromeOsOptionValueType>
mojo::StructPtr<MojoOptionType> PrintOptionToMojom(
    const chromeos::Printer::PrintOption<ChromeOsOptionValueType>&
        print_option) {
  return PrintOptionToMojom<MojoOptionType>(
      print_option,
      base::BindRepeating([](const ChromeOsOptionValueType& value) {
        return static_cast<MojoOptionValueType>(value);
      }));
}

}  // namespace

crosapi::mojom::LocalPrinter* GetLocalPrinterInterface() {
  CHECK(crosapi::CrosapiManager::IsInitialized());
  return crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
}

crosapi::mojom::CapabilitiesResponsePtr PrinterWithCapabilitiesToMojom(
    const chromeos::Printer& printer,
    const std::optional<printing::PrinterSemanticCapsAndDefaults>& caps) {
  return crosapi::mojom::CapabilitiesResponse::New(
      PrinterToMojom(printer), printer.HasSecureProtocol(),
      caps,     // comment to prevent git cl format
      0, 0, 0,  // deprecated
      printing::mojom::PinModeRestriction::kUnset,     // deprecated
      printing::mojom::ColorModeRestriction::kUnset,   // deprecated
      printing::mojom::DuplexModeRestriction::kUnset,  // deprecated
      printing::mojom::PinModeRestriction::kUnset);    // deprecated
}

crosapi::mojom::LocalDestinationInfoPtr PrinterToMojom(
    const chromeos::Printer& printer) {
  return crosapi::mojom::LocalDestinationInfo::New(
      printer.id(), printer.display_name(), printer.description(),
      printer.source() == chromeos::Printer::SRC_POLICY,
      printer.uri().GetNormalized(/*always_print_port=*/true),
      StatusToMojom(printer.printer_status()),
      ManagedPrintOptionsToMojom(printer.print_job_options()));
}

crosapi::mojom::PrinterStatusPtr StatusToMojom(
    const chromeos::CupsPrinterStatus& status) {
  auto ptr = crosapi::mojom::PrinterStatus::New();
  ptr->printer_id = status.GetPrinterId();
  ptr->timestamp = status.GetTimestamp();
  for (const auto& reason : status.GetStatusReasons()) {
    if (reason.GetReason() == crosapi::mojom::StatusReason::Reason::kNoError) {
      continue;
    }
    ptr->status_reasons.push_back(crosapi::mojom::StatusReason::New(
        reason.GetReason(), reason.GetSeverity()));
  }
  return ptr;
}

crosapi::mojom::ManagedPrintOptionsPtr ManagedPrintOptionsToMojom(
    const chromeos::Printer::ManagedPrintOptions& managed_print_options) {
  auto result = crosapi::mojom::ManagedPrintOptions::New();

  result->media_size = PrintOptionToMojom<crosapi::mojom::SizeOption>(
      managed_print_options.media_size,
      base::BindRepeating([](const chromeos::Printer::Size& value) {
        return crosapi::mojom::Size::New(value.width, value.height);
      }));

  result->media_type = PrintOptionToMojom<crosapi::mojom::StringOption>(
      managed_print_options.media_type);

  result->duplex = PrintOptionToMojom<crosapi::mojom::DuplexOption>(
      managed_print_options.duplex,
      base::BindRepeating([](const chromeos::Printer::DuplexType& value) {
        switch (value) {
          case chromeos::Printer::DuplexType::kOneSided:
            return crosapi::mojom::DuplexType::kOneSided;
          case chromeos::Printer::DuplexType::kShortEdge:
            return crosapi::mojom::DuplexType::kShortEdge;
          case chromeos::Printer::DuplexType::kLongEdge:
            return crosapi::mojom::DuplexType::kLongEdge;
          default:
            return crosapi::mojom::DuplexType::kUnknownDuplex;
        }
      }));

  result->color = PrintOptionToMojom<crosapi::mojom::BoolOption>(
      managed_print_options.color);

  result->dpi = PrintOptionToMojom<crosapi::mojom::DpiOption>(
      managed_print_options.dpi,
      base::BindRepeating([](const chromeos::Printer::Dpi& value) {
        return crosapi::mojom::Dpi::New(value.horizontal, value.vertical);
      }));

  result->quality = PrintOptionToMojom<crosapi::mojom::QualityOption>(
      managed_print_options.quality,
      base::BindRepeating([](const chromeos::Printer::QualityType& value) {
        switch (value) {
          case chromeos::Printer::QualityType::kDraft:
            return crosapi::mojom::QualityType::kDraft;
          case chromeos::Printer::QualityType::kNormal:
            return crosapi::mojom::QualityType::kNormal;
          case chromeos::Printer::QualityType::kHigh:
            return crosapi::mojom::QualityType::kHigh;
          default:
            return crosapi::mojom::QualityType::kUnknownQuality;
        }
      }));

  result->print_as_image = PrintOptionToMojom<crosapi::mojom::BoolOption>(
      managed_print_options.print_as_image);

  return result;
}

}  // namespace printing
