// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_input_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdInputSamplerHandler::~CrosHealthdInputSamplerHandler() = default;

void CrosHealthdInputSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  std::optional<MetricData> metric_data;
  const auto& input_result = result->input_result;

  if (!input_result.is_null()) {
    switch (input_result->which()) {
      case cros_healthd::InputResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting input info: "
                 << input_result->get_error()->msg;
        break;
      }

      case cros_healthd::InputResult::Tag::kInputInfo: {
        const auto& input_info = input_result->get_input_info();
        if (input_info.is_null()) {
          DVLOG(1) << "Null InputInfo from cros_healthd";
          break;
        }

        // Gather touch screen info.
        metric_data = std::make_optional<MetricData>();
        auto* const touch_screen_info_out =
            metric_data->mutable_info_data()->mutable_touch_screen_info();

        touch_screen_info_out->set_library_name(
            input_info->touchpad_library_name);

        for (const auto& screen : input_info->touchscreen_devices) {
          if (screen->input_device->is_enabled &&
              screen->input_device->connection_type ==
                  cros_healthd::InputDevice::ConnectionType::kInternal) {
            auto* const touch_screen_device_out =
                touch_screen_info_out->add_touch_screen_devices();
            touch_screen_device_out->set_display_name(
                screen->input_device->name);
            touch_screen_device_out->set_touch_points(screen->touch_points);
            touch_screen_device_out->set_has_stylus(screen->has_stylus);
          }
        }
        // Don't report anything if no internal touchscreen was detected.
        if (touch_screen_info_out->touch_screen_devices().empty()) {
          metric_data = std::nullopt;
        }
        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

}  // namespace reporting