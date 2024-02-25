// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_display_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdDisplaySamplerHandler::CrosHealthdDisplaySamplerHandler(
    MetricType metric_type)
    : metric_type_(metric_type) {}

CrosHealthdDisplaySamplerHandler::~CrosHealthdDisplaySamplerHandler() = default;

void CrosHealthdDisplaySamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  std::optional<MetricData> metric_data;
  const auto& display_result = result->display_result;
  if (!display_result.is_null()) {
    switch (display_result->which()) {
      case cros_healthd::DisplayResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting bus info: "
                 << display_result->get_error()->msg;
        break;
      }

      case cros_healthd::DisplayResult::Tag::kDisplayInfo: {
        const auto& display_info = display_result->get_display_info();
        if (display_info.is_null()) {
          DVLOG(1) << "Null DisplayInfo from cros_healthd";
          break;
        }

        metric_data = std::make_optional<MetricData>();
        const auto* const embedded_display_info =
            display_info->embedded_display.get();
        if (metric_type_ == MetricType::kInfo) {
          // Gather e-privacy screen info.
          auto* const privacy_screen_info_out =
              metric_data->mutable_info_data()->mutable_privacy_screen_info();
          privacy_screen_info_out->set_supported(
              embedded_display_info->privacy_screen_supported);

          // Gather displays info.
          auto* const internal_dp_out = metric_data->mutable_info_data()
                                            ->mutable_display_info()
                                            ->add_display_device();
          internal_dp_out->set_is_internal(true);
          if (embedded_display_info->display_name.has_value()) {
            internal_dp_out->set_display_name(
                embedded_display_info->display_name.value());
          }
          if (embedded_display_info->display_width) {
            internal_dp_out->set_display_width(
                embedded_display_info->display_width->value);
          }
          if (embedded_display_info->display_height) {
            internal_dp_out->set_display_height(
                embedded_display_info->display_height->value);
          }
          if (embedded_display_info->manufacturer.has_value()) {
            internal_dp_out->set_manufacturer(
                embedded_display_info->manufacturer.value());
          }
          if (embedded_display_info->model_id) {
            internal_dp_out->set_model_id(
                embedded_display_info->model_id->value);
          }
          if (embedded_display_info->manufacture_year) {
            internal_dp_out->set_manufacture_year(
                embedded_display_info->manufacture_year->value);
          }
          if (display_info->external_displays) {
            for (const auto& current_external_display :
                 *display_info->external_displays) {
              auto* const external_dp_out = metric_data->mutable_info_data()
                                                ->mutable_display_info()
                                                ->add_display_device();
              external_dp_out->set_is_internal(false);
              if (current_external_display->display_name.has_value()) {
                external_dp_out->set_display_name(
                    current_external_display->display_name.value());
              }
              if (current_external_display->display_width) {
                external_dp_out->set_display_width(
                    current_external_display->display_width->value);
              }
              if (current_external_display->display_height) {
                external_dp_out->set_display_height(
                    current_external_display->display_height->value);
              }
              if (current_external_display->manufacturer.has_value()) {
                external_dp_out->set_manufacturer(
                    current_external_display->manufacturer.value());
              }
              if (current_external_display->model_id) {
                external_dp_out->set_model_id(
                    current_external_display->model_id->value);
              }
              if (current_external_display->manufacture_year) {
                external_dp_out->set_manufacture_year(
                    current_external_display->manufacture_year->value);
              }
            }
          }
        } else if (metric_type_ == MetricType::kTelemetry) {
          // Gather displays telemetry.
          auto* const internal_dp_out = metric_data->mutable_telemetry_data()
                                            ->mutable_displays_telemetry()
                                            ->add_display_status();
          internal_dp_out->set_is_internal(true);
          if (embedded_display_info->display_name.has_value()) {
            internal_dp_out->set_display_name(
                embedded_display_info->display_name.value());
          }
          if (embedded_display_info->resolution_horizontal) {
            internal_dp_out->set_resolution_horizontal(
                embedded_display_info->resolution_horizontal->value);
          }
          if (embedded_display_info->resolution_vertical) {
            internal_dp_out->set_resolution_vertical(
                embedded_display_info->resolution_vertical->value);
          }
          if (embedded_display_info->refresh_rate) {
            internal_dp_out->set_refresh_rate(
                embedded_display_info->refresh_rate->value);
          }
          if (display_info->external_displays) {
            for (const auto& current_external_display :
                 *display_info->external_displays) {
              auto* const external_dp_out =
                  metric_data->mutable_telemetry_data()
                      ->mutable_displays_telemetry()
                      ->add_display_status();
              external_dp_out->set_is_internal(false);
              if (current_external_display->display_name.has_value()) {
                external_dp_out->set_display_name(
                    current_external_display->display_name.value());
              }
              if (current_external_display->resolution_horizontal) {
                external_dp_out->set_resolution_horizontal(
                    current_external_display->resolution_horizontal->value);
              }
              if (current_external_display->resolution_vertical) {
                external_dp_out->set_resolution_vertical(
                    current_external_display->resolution_vertical->value);
              }
              if (current_external_display->refresh_rate) {
                external_dp_out->set_refresh_rate(
                    current_external_display->refresh_rate->value);
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
