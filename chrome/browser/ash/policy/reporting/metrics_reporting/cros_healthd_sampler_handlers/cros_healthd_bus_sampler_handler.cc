// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_bus_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

namespace {

ThunderboltSecurityLevel TranslateThunderboltSecurityLevel(
    cros_healthd::ThunderboltSecurityLevel security_level) {
  switch (security_level) {
    case cros_healthd::ThunderboltSecurityLevel::kNone:
      return THUNDERBOLT_SECURITY_NONE_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kUserLevel:
      return THUNDERBOLT_SECURITY_USER_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kSecureLevel:
      return THUNDERBOLT_SECURITY_SECURE_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kDpOnlyLevel:
      return THUNDERBOLT_SECURITY_DP_ONLY_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kUsbOnlyLevel:
      return THUNDERBOLT_SECURITY_USB_ONLY_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kNoPcieLevel:
      return THUNDERBOLT_SECURITY_NO_PCIE_LEVEL;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace

CrosHealthdBusSamplerHandler::CrosHealthdBusSamplerHandler(
    MetricType metric_type)
    : metric_type_(metric_type) {}

CrosHealthdBusSamplerHandler::~CrosHealthdBusSamplerHandler() = default;

void CrosHealthdBusSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  std::optional<MetricData> metric_data;
  const auto& bus_result = result->bus_result;

  if (!bus_result.is_null()) {
    switch (bus_result->which()) {
      case cros_healthd::BusResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting bus info: "
                 << bus_result->get_error()->msg;
        break;
      }

      case cros_healthd::BusResult::Tag::kBusDevices: {
        for (const auto& bus_device : bus_result->get_bus_devices()) {
          const auto& bus_info = bus_device->bus_info;
          if (metric_type_ == MetricType::kInfo) {
            if (bus_info->is_thunderbolt_bus_info()) {
              if (!metric_data.has_value()) {
                metric_data = std::make_optional<MetricData>();
              }
              auto* const thunderbolt_info_out =
                  metric_data->mutable_info_data()
                      ->mutable_bus_device_info()
                      ->add_thunderbolt_info();
              thunderbolt_info_out->set_security_level(
                  TranslateThunderboltSecurityLevel(
                      bus_info->get_thunderbolt_bus_info()->security_level));
            }
          } else if (metric_type_ == MetricType::kTelemetry) {
            if (bus_info->is_usb_bus_info()) {
              if (!metric_data.has_value()) {
                metric_data = std::make_optional<MetricData>();
              }
              auto* const usb_telemetry_out =
                  metric_data->mutable_telemetry_data()
                      ->mutable_peripherals_telemetry()
                      ->add_usb_telemetry();
              usb_telemetry_out->set_vid(
                  bus_info->get_usb_bus_info()->vendor_id);
              usb_telemetry_out->set_pid(
                  bus_info->get_usb_bus_info()->product_id);
              usb_telemetry_out->set_class_id(
                  bus_info->get_usb_bus_info()->class_id);
              usb_telemetry_out->set_subclass_id(
                  bus_info->get_usb_bus_info()->subclass_id);
              usb_telemetry_out->set_vendor(bus_device->vendor_name);
              usb_telemetry_out->set_name(bus_device->product_name);
              if (bus_info->get_usb_bus_info()->fwupd_firmware_version_info) {
                usb_telemetry_out->set_firmware_version(
                    bus_info->get_usb_bus_info()
                        ->fwupd_firmware_version_info->version);
              }
            }
          }
        }
        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

}  // namespace reporting
