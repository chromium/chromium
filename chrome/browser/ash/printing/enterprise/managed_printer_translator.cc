// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/managed_printer_translator.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/managed_printer_configuration.pb.h"
#include "chrome/browser/ash/printing/enterprise/print_job_options_translator.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Top-level field names.
const char kGuid[] = "guid";
const char kDisplayName[] = "display_name";
const char kDescription[] = "description";
const char kUri[] = "uri";
const char kUsbDeviceId[] = "usb_device_id";
const char kPpdResource[] = "ppd_resource";
const char kPrintJobOptions[] = "print_job_options";

// PpdResource field names.
const char kUserSuppliedPpdUri[] = "user_supplied_ppd_uri";
const char kEffectiveModel[] = "effective_model";
const char kAutoconf[] = "autoconf";

// UsbDeviceId field names.
const char kVendorId[] = "vendor_id";
const char kProductId[] = "product_id";
const char kUsbProtocol[] = "usb_protocol";

std::optional<ManagedPrinterConfiguration::PpdResource> PpdResourceFromDict(
    const base::Value::Dict& ppd_resource) {
  std::optional<bool> autoconf = ppd_resource.FindBool(kAutoconf);
  const std::string* effective_model = ppd_resource.FindString(kEffectiveModel);
  const std::string* user_supplied_ppd_uri =
      ppd_resource.FindString(kUserSuppliedPpdUri);

  bool has_autoconf = autoconf.has_value();
  bool has_effective_model = (effective_model != nullptr);
  bool has_user_supplied_ppd_uri = (user_supplied_ppd_uri != nullptr);

  bool has_one_ppd_resource =
      (has_autoconf + has_effective_model + has_user_supplied_ppd_uri) == 1;
  if (!has_one_ppd_resource) {
    LOG(WARNING) << base::StringPrintf(
        "Could not convert a dictionary to PpdResource: multiple values set "
        "for the 'resource' oneof field: %s",
        ppd_resource.DebugString().c_str());
    return std::nullopt;
  }

  ManagedPrinterConfiguration::PpdResource result;
  if (autoconf.has_value()) {
    result.set_autoconf(autoconf.value());
  }
  if (effective_model) {
    result.set_effective_model(*effective_model);
  }
  if (user_supplied_ppd_uri) {
    result.set_user_supplied_ppd_uri(*user_supplied_ppd_uri);
  }

  return result;
}

std::optional<Printer::PpdReference> ManagedPpdResourceToPpdReference(
    const ManagedPrinterConfiguration::PpdResource& ppd_resource) {
  Printer::PpdReference ppd_reference;
  switch (ppd_resource.resource_case()) {
    case ManagedPrinterConfiguration::PpdResource::kAutoconf: {
      if (!ppd_resource.autoconf()) {
        LOG(WARNING) << base::StringPrintf(
            "Invalid PPD resource - autoconf must either be unset or set to "
            "true: %s",
            ppd_resource.SerializeAsString().c_str());
        return std::nullopt;
      }
      ppd_reference.autoconf = ppd_resource.autoconf();
      break;
    }
    case ManagedPrinterConfiguration::PpdResource::kUserSuppliedPpdUri: {
      GURL url(ppd_resource.user_supplied_ppd_uri());
      if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
        LOG(WARNING) << base::StringPrintf(
            "Invalid PPD resource - invalid user_supplied_ppd_url resource: %s",
            ppd_resource.SerializeAsString().c_str());
        return std::nullopt;
      }
      ppd_reference.user_supplied_ppd_url =
          ppd_resource.user_supplied_ppd_uri();
      break;
    }
    case ManagedPrinterConfiguration::PpdResource::kEffectiveModel: {
      ppd_reference.effective_make_and_model = ppd_resource.effective_model();
      break;
    }
    case ManagedPrinterConfiguration::PpdResource::RESOURCE_NOT_SET: {
      LOG(WARNING) << "Invalid PPD resource - resource is not set";
      return std::nullopt;
    }
  }
  return ppd_reference;
}

std::optional<Printer::UsbDeviceId> UsbDeviceIdFromInts(int vendor_id,
                                                        int product_id) {
  // Verify values are in the uint16 range.
  if (vendor_id < std::numeric_limits<uint16_t>::min() ||
      vendor_id > std::numeric_limits<uint16_t>::max() ||
      product_id < std::numeric_limits<uint16_t>::min() ||
      product_id > std::numeric_limits<uint16_t>::max()) {
    LOG(WARNING) << "vendor_id or product_id out of range: " << vendor_id
                 << ", " << product_id;
    return std::nullopt;
  }

  return Printer::UsbDeviceId(static_cast<uint16_t>(vendor_id),
                              static_cast<uint16_t>(product_id));
}

std::optional<ManagedPrinterConfiguration::UsbDeviceId>
UsbDeviceIdProtoFromDict(const base::Value::Dict& dict) {
  std::optional<int> vendor_id = dict.FindInt(kVendorId);
  std::optional<int> product_id = dict.FindInt(kProductId);
  std::optional<int> usb_protocol = dict.FindInt(kUsbProtocol);

  // Verify values exist and are integers.
  if (!vendor_id.has_value() || !product_id.has_value()) {
    LOG(WARNING) << "vendor_id or product_id missing or not an int: "
                 << dict.DebugString();
    return std::nullopt;
  }

  // Verify usb_protocol exists and is an integer.
  if (!usb_protocol.has_value()) {
    LOG(WARNING) << "usb_protocol missing or not an int: "
                 << dict.DebugString();
    return std::nullopt;
  }

  auto usb_device_id =
      UsbDeviceIdFromInts(vendor_id.value(), product_id.value());
  if (!usb_device_id.has_value()) {
    return std::nullopt;
  }

  if (!ManagedPrinterConfiguration_UsbProtocol_IsValid(usb_protocol.value()) ||
      usb_protocol.value() ==
          ManagedPrinterConfiguration_UsbProtocol::
              ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_UNSPECIFIED) {
    LOG(WARNING) << "UsbProtocol value invalid: " << usb_protocol.value();
    return std::nullopt;
  }

  auto usb_device_id_proto = ManagedPrinterConfiguration::UsbDeviceId();
  usb_device_id_proto.set_vendor_id(usb_device_id.value().vendor_id);
  usb_device_id_proto.set_product_id(usb_device_id.value().product_id);
  usb_device_id_proto.set_usb_protocol(
      static_cast<ManagedPrinterConfiguration_UsbProtocol>(
          usb_protocol.value()));

  return usb_device_id_proto;
}

}  // namespace

std::optional<ManagedPrinterConfiguration> ManagedPrinterConfigFromDict(
    const base::Value::Dict& config) {
  const std::string* guid = config.FindString(kGuid);
  const std::string* display_name = config.FindString(kDisplayName);
  const std::string* description = config.FindString(kDescription);
  const std::string* uri = config.FindString(kUri);
  const base::Value::Dict* usb_device_id_dict = config.FindDict(kUsbDeviceId);
  const base::Value::Dict* ppd_resource = config.FindDict(kPpdResource);
  const base::Value::Dict* print_job_options =
      config.FindDict(kPrintJobOptions);

  ManagedPrinterConfiguration result;
  if (guid) {
    result.set_guid(*guid);
  }
  if (display_name) {
    result.set_display_name(*display_name);
  }
  if (uri) {
    if (usb_device_id_dict) {
      LOG(WARNING) << base::StringPrintf(
          "Could not convert a dictionary to ManagedPrinterConfiguration: "
          "multiple values set for the 'connection_type' oneof field: %s",
          config.DebugString().c_str());
      return std::nullopt;
    }
    result.set_uri(*uri);
  }
  if (usb_device_id_dict) {
    auto usb_device_id = UsbDeviceIdProtoFromDict(*usb_device_id_dict);
    if (!usb_device_id.has_value()) {
      LOG(WARNING) << base::StringPrintf(
          "Could not convert a dictionary to UsbDeviceId: "
          "invalid 'usb_device_id' field: %s",
          usb_device_id_dict->DebugString().c_str());
      return std::nullopt;
    }
    *result.mutable_usb_device_id() = usb_device_id.value();
  }
  if (description) {
    result.set_description(*description);
  }
  if (ppd_resource) {
    auto ppd_resource_opt = PpdResourceFromDict(*ppd_resource);
    if (!ppd_resource_opt) {
      LOG(WARNING) << base::StringPrintf(
          "Could not convert a dictionary to ManagedPrinterConfiguration: "
          "invalid 'ppd_resource' field: %s",
          config.DebugString().c_str());
      return std::nullopt;
    }
    *result.mutable_ppd_resource() = *ppd_resource_opt;
  }
  if (print_job_options) {
    *result.mutable_print_job_options() =
        ManagedPrintOptionsProtoFromDict(*print_job_options);
  }
  return result;
}

std::optional<Printer> PrinterFromManagedPrinterConfig(
    const ManagedPrinterConfiguration& managed_printer) {
  static auto LogRequiredFieldMissing = [](std::string_view field) {
    LOG(WARNING) << "Managed printer is missing required field: " << field;
  };

  if (!managed_printer.has_guid()) {
    LogRequiredFieldMissing(kGuid);
    return std::nullopt;
  }
  if (!managed_printer.has_display_name()) {
    LogRequiredFieldMissing(kDisplayName);
    return std::nullopt;
  }
  if (!managed_printer.has_ppd_resource()) {
    LogRequiredFieldMissing(kPpdResource);
    return std::nullopt;
  }

  Printer printer(managed_printer.guid());
  printer.set_source(Printer::SRC_POLICY);
  printer.set_display_name(managed_printer.display_name());

  if (managed_printer.has_uri()) {
    std::string set_uri_error_message;
    if (!printer.SetUri(managed_printer.uri(), &set_uri_error_message)) {
      LOG(WARNING) << base::StringPrintf(
          "Managed printer '%s' has invalid %s value: %s, error: %s",
          managed_printer.display_name().c_str(), kUri,
          managed_printer.uri().c_str(), set_uri_error_message.c_str());
      return std::nullopt;
    }
  } else if (managed_printer.has_usb_device_id()) {
    std::optional<Printer::UsbDeviceId> usb_device_id =
        UsbDeviceIdFromInts(managed_printer.usb_device_id().vendor_id(),
                            managed_printer.usb_device_id().product_id());
    if (!usb_device_id.has_value()) {
      LOG(WARNING) << base::StringPrintf(
          "Managed printer '%s' has invalid %s value: %d, %d",
          managed_printer.display_name().c_str(), kUsbDeviceId,
          managed_printer.usb_device_id().vendor_id(),
          managed_printer.usb_device_id().product_id());
      return std::nullopt;
    }
    ManagedPrinterConfiguration_UsbProtocol usb_protocol =
        managed_printer.usb_device_id().usb_protocol();
    if (!ManagedPrinterConfiguration_UsbProtocol_IsValid(usb_protocol) ||
        usb_protocol ==
            ManagedPrinterConfiguration_UsbProtocol::
                ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_UNSPECIFIED) {
      LOG(WARNING) << base::StringPrintf(
          "Managed printer '%s' has invalid %s value: %d",
          managed_printer.display_name().c_str(), kUsbProtocol, usb_protocol);
      return std::nullopt;
    }
    printer.set_usb_device_id(usb_device_id.value());
    // Also set the URI since it's needed by CUPS and displayed in the UI.
    if (usb_protocol ==
        ManagedPrinterConfiguration_UsbProtocol::
            ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_LEGACY_USB) {
      printer.SetUri(base::StringPrintf("usb://%04x/%04x?serial",
                                        usb_device_id.value().vendor_id,
                                        usb_device_id.value().product_id));
    } else {
      printer.SetUri(base::StringPrintf("ippusb://%04x_%04x/ipp/print",
                                        usb_device_id.value().vendor_id,
                                        usb_device_id.value().product_id));
    }
  } else {
    LOG(WARNING) << "Managed printer is missing oneof field: " << kUri << " or "
                 << kUsbDeviceId;
    return std::nullopt;
  }

  auto ppd_reference =
      ManagedPpdResourceToPpdReference(managed_printer.ppd_resource());
  if (!ppd_reference.has_value()) {
    LOG(WARNING) << base::StringPrintf(
        "Managed printer '%s' has invalid %s value: %s",
        managed_printer.display_name().c_str(), kPpdResource,
        managed_printer.ppd_resource().SerializeAsString().c_str());
    return std::nullopt;
  }
  *printer.mutable_ppd_reference() = *ppd_reference;

  if (managed_printer.has_description()) {
    printer.set_description(managed_printer.description());
  }

  if (managed_printer.has_print_job_options()) {
    std::optional<Printer::ManagedPrintOptions> print_job_options =
        ChromeOsPrintOptionsFromManagedPrintOptions(
            managed_printer.print_job_options());
    if (!print_job_options) {
      LOG(WARNING) << base::StringPrintf(
          "Managed printer '%s' has invalid %s value: %s",
          managed_printer.display_name().c_str(), kPrintJobOptions,
          managed_printer.print_job_options().SerializeAsString().c_str());
      return std::nullopt;
    }
    printer.set_print_job_options(print_job_options.value());
  }

  return printer;
}

}  // namespace chromeos
