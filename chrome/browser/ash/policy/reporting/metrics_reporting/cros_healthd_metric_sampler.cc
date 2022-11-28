// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include "base/logging.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_audio_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_boot_performance_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_bus_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_cpu_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_display_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_input_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_memory_sampler_handler.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace {

namespace cros_healthd = ::ash::cros_healthd::mojom;

void OnHealthdInfoReceived(OptionalMetricCallback callback,
                           cros_healthd::ProbeCategoryEnum probe_category,
                           CrosHealthdMetricSampler::MetricType metric_type,
                           cros_healthd::TelemetryInfoPtr result) {
  DCHECK(result);
  switch (probe_category) {
    case cros_healthd::ProbeCategoryEnum::kAudio: {
      CrosHealthdAudioSamplerHandler handler = CrosHealthdAudioSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBus: {
      CrosHealthdBusSamplerHandler handler = CrosHealthdBusSamplerHandler(metric_type);
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kCpu: {
      CrosHealthdCpuSamplerHandler handler = CrosHealthdCpuSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kMemory: {
      CrosHealthdMemorySamplerHandler handler = CrosHealthdMemorySamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBootPerformance: {
      CrosHealthdBootPerformanceSamplerHandler handler =
          CrosHealthdBootPerformanceSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kInput: {
      CrosHealthdInputSamplerHandler handler = CrosHealthdInputSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kDisplay: {
      auto handler = CrosHealthdDisplaySamplerHandler(metric_type);
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    default: {
      NOTREACHED();
      return;
    }
  }
}

}  // namespace

CrosHealthdMetricSampler::CrosHealthdMetricSampler(
    cros_healthd::ProbeCategoryEnum probe_category,
    CrosHealthdMetricSampler::MetricType metric_type)
    : probe_category_(probe_category), metric_type_(metric_type) {}

CrosHealthdMetricSampler::~CrosHealthdMetricSampler() = default;

void CrosHealthdMetricSampler::MaybeCollect(OptionalMetricCallback callback) {
  auto healthd_callback =
      base::BindOnce(OnHealthdInfoReceived, std::move(callback),
                     probe_category_, metric_type_);
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          std::vector<cros_healthd::ProbeCategoryEnum>{probe_category_},
          std::move(healthd_callback));
}

}  // namespace reporting
