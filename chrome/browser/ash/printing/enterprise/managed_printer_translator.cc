// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/managed_printer_translator.h"

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/managed_printer_configuration.pb.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Top-level field names.
const char kDisplayName[] = "display_name";
const char kDescription[] = "description";
const char kUri[] = "uri";
const char kPpdResource[] = "ppd_resource";
const char kGuid[] = "guid";

// PpdResource field names.
const char kUserSuppliedPpdUri[] = "user_supplied_ppd_uri";
const char kEffectiveModel[] = "effective_model";
const char kAutoconf[] = "autoconf";

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

}  // namespace

std::optional<ManagedPrinterConfiguration> ManagedPrinterConfigFromDict(
    const base::Value::Dict& config) {
  const std::string* guid = config.FindString(kGuid);
  const std::string* display_name = config.FindString(kDisplayName);
  const std::string* uri = config.FindString(kUri);
  const base::Value::Dict* ppd_resource = config.FindDict(kPpdResource);
  const std::string* description = config.FindString(kDescription);

  ManagedPrinterConfiguration result;
  if (guid) {
    result.set_guid(*guid);
  }
  if (display_name) {
    result.set_display_name(*display_name);
  }
  if (uri) {
    result.set_uri(*uri);
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
  if (!managed_printer.has_uri()) {
    LogRequiredFieldMissing(kUri);
    return std::nullopt;
  }
  if (!managed_printer.has_ppd_resource()) {
    LogRequiredFieldMissing(kPpdResource);
    return std::nullopt;
  }

  Printer printer(managed_printer.guid());
  printer.set_source(Printer::SRC_POLICY);
  printer.set_display_name(managed_printer.display_name());
  std::string set_uri_error_message;
  if (!printer.SetUri(managed_printer.uri(), &set_uri_error_message)) {
    LOG(WARNING) << base::StringPrintf(
        "Managed printer '%s' has invalid %s value: %s, error: %s",
        managed_printer.display_name().c_str(), kUri,
        managed_printer.uri().c_str(), set_uri_error_message.c_str());
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
  return printer;
}

}  // namespace chromeos
