// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/fake_probe_service.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom-shared.h"
#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/probe_service.h"
#include "ash/webui/telemetry_extension_ui/services/probe_service_converters.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

FakeProbeService::FakeProbeService(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver,
    health::mojom::TelemetryInfoPtr telem_info,
    health::mojom::OemDataPtr oem_data,
    std::vector<health::mojom::ProbeCategoryEnum>* requested_categories)
    : receiver_(this, std::move(receiver)),
      telem_info_(std::move(telem_info)),
      oem_data_(std::move(oem_data)),
      requested_categories_(requested_categories) {}

FakeProbeService::~FakeProbeService() = default;

void FakeProbeService::ProbeTelemetryInfo(
    const std::vector<health::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  DCHECK(requested_categories_->empty());
  requested_categories_->insert(requested_categories_->end(),
                                categories.begin(), categories.end());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), telem_info_.Clone()));
}

void FakeProbeService::GetOemData(GetOemDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), oem_data_.Clone()));
}

void FakeProbeService::Factory::SetProbeTelemetryInfoResponseForTesting(
    health::mojom::TelemetryInfoPtr response_info) {
  telem_info_ = std::move(response_info);
}

void FakeProbeService::Factory::SetOemDataResponseForTesting(
    health::mojom::OemDataPtr oem_data) {
  oem_data_ = std::move(oem_data);
}

std::vector<health::mojom::ProbeCategoryEnum>
FakeProbeService::Factory::GetAndClearRequestedCategories() {
  return std::move(requested_categories_);
}

std::unique_ptr<health::mojom::ProbeService>
FakeProbeService::Factory::CreateInstance(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver) {
  return base::WrapUnique<FakeProbeService>(
      new FakeProbeService(std::move(receiver), telem_info_->Clone(),
                           oem_data_.Clone(), &requested_categories_));
}

FakeProbeService::Factory::Factory() = default;
FakeProbeService::Factory::~Factory() {
  DCHECK(requested_categories_.empty())
      << "FakeProbeService::Factory::CheckAndClearRequestedCategories has not "
         "been called, you probably forgot to check the requested categories";
};

}  // namespace ash
