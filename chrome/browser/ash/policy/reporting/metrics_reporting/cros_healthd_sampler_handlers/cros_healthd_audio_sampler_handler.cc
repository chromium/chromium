// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_audio_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

CrosHealthdAudioSamplerHandler::~CrosHealthdAudioSamplerHandler() = default;

void CrosHealthdAudioSamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  std::optional<MetricData> metric_data;
  const auto& audio_result = result->audio_result;

  if (!audio_result.is_null()) {
    switch (audio_result->which()) {
      case cros_healthd::AudioResult::Tag::kError: {
        DVLOG(1) << "CrosHealthD: Error getting audio telemetry: "
                 << audio_result->get_error()->msg;
        break;
      }

      case cros_healthd::AudioResult::Tag::kAudioInfo: {
        const auto& audio_info = audio_result->get_audio_info();
        if (audio_info.is_null()) {
          DVLOG(1) << "CrosHealthD: No audio info received";
          break;
        }

        metric_data = std::make_optional<MetricData>();
        auto* const audio_info_out =
            metric_data->mutable_telemetry_data()->mutable_audio_telemetry();
        audio_info_out->set_output_mute(audio_info->output_mute);
        audio_info_out->set_input_mute(audio_info->input_mute);
        audio_info_out->set_output_volume(audio_info->output_volume);
        audio_info_out->set_output_device_name(audio_info->output_device_name);
        audio_info_out->set_input_gain(audio_info->input_gain);
        audio_info_out->set_input_device_name(audio_info->input_device_name);

        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

}  // namespace reporting
