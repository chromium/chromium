// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_memory_sampler_handler.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

namespace {

MemoryEncryptionState TranslateMemoryEncryptionState(
    cros_healthd::EncryptionState encryption_state) {
  switch (encryption_state) {
    case cros_healthd::EncryptionState::kUnknown:
      return MEMORY_ENCRYPTION_STATE_UNKNOWN;
    case cros_healthd::EncryptionState::kEncryptionDisabled:
      return MEMORY_ENCRYPTION_STATE_DISABLED;
    case cros_healthd::EncryptionState::kTmeEnabled:
      return MEMORY_ENCRYPTION_STATE_TME;
    case cros_healthd::EncryptionState::kMktmeEnabled:
      return MEMORY_ENCRYPTION_STATE_MKTME;
  }

  NOTREACHED_IN_MIGRATION();
}

MemoryEncryptionAlgorithm TranslateMemoryEncryptionAlgorithm(
    cros_healthd::CryptoAlgorithm encryption_algorithm) {
  switch (encryption_algorithm) {
    case cros_healthd::CryptoAlgorithm::kUnknown:
      return MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN;
    case cros_healthd::CryptoAlgorithm::kAesXts128:
      return MEMORY_ENCRYPTION_ALGORITHM_AES_XTS_128;
    case cros_healthd::CryptoAlgorithm::kAesXts256:
      return MEMORY_ENCRYPTION_ALGORITHM_AES_XTS_256;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace

CrosHealthdMemorySamplerHandler::~CrosHealthdMemorySamplerHandler() = default;

void CrosHealthdMemorySamplerHandler::HandleResult(
    OptionalMetricCallback callback,
    cros_healthd::TelemetryInfoPtr result) const {
  std::optional<MetricData> metric_data;
  const auto& memory_result = result->memory_result;

  if (!memory_result.is_null()) {
    switch (memory_result->which()) {
      case cros_healthd::MemoryResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting memory info: "
                 << memory_result->get_error()->msg;
        break;
      }

      case cros_healthd::MemoryResult::Tag::kMemoryInfo: {
        const auto& memory_info = memory_result->get_memory_info();
        if (memory_result.is_null()) {
          DVLOG(1) << "Null MemoryInfo from cros_healthd";
          break;
        }

        // Gather memory info.
        metric_data = std::make_optional<MetricData>();
        auto* const memory_encryption_info_out =
            metric_data->mutable_info_data()
                ->mutable_memory_info()
                ->mutable_tme_info();
        const auto* const memory_encryption_info =
            memory_info->memory_encryption_info.get();

        if (memory_encryption_info) {
          memory_encryption_info_out->set_encryption_state(
              TranslateMemoryEncryptionState(
                  memory_encryption_info->encryption_state));
          memory_encryption_info_out->set_encryption_algorithm(
              TranslateMemoryEncryptionAlgorithm(
                  memory_encryption_info->active_algorithm));
          memory_encryption_info_out->set_max_keys(
              memory_encryption_info->max_key_number);
          memory_encryption_info_out->set_key_length(
              memory_encryption_info->key_length);
        } else {
          // If encryption info isn't set, mark it as disabled.
          memory_encryption_info_out->set_encryption_state(
              MEMORY_ENCRYPTION_STATE_DISABLED);
        }
        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

}  // namespace reporting
