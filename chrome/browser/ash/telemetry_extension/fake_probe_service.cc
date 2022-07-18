// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/fake_probe_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

void FakeProbeService::Factory::SetCreateInstanceResponse(
    std::unique_ptr<FakeProbeService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<health::mojom::ProbeService>
FakeProbeService::Factory::CreateInstance(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver) {
  fake_service_->BindPendingReceiver(std::move(receiver));
  return std::move(fake_service_);
}

FakeProbeService::Factory::Factory() = default;
FakeProbeService::Factory::~Factory() = default;

FakeProbeService::FakeProbeService() : receiver_(this) {}
FakeProbeService::~FakeProbeService() {
  // Assert on the expectations.
  EXPECT_EQ(actual_requested_categories_, expected_requested_categories_);
}

void FakeProbeService::ProbeTelemetryInfo(
    const std::vector<health::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  actual_requested_categories_.clear();
  actual_requested_categories_.insert(actual_requested_categories_.end(),
                                      categories.begin(), categories.end());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), telem_info_.Clone()));
}

void FakeProbeService::GetOemData(GetOemDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), oem_data_.Clone()));
}

void FakeProbeService::SetExpectedLastRequestedCategories(
    std::vector<health::mojom::ProbeCategoryEnum>
        expected_requested_categories) {
  expected_requested_categories_ = std::move(expected_requested_categories);
}

void FakeProbeService::SetProbeTelemetryInfoResponse(
    health::mojom::TelemetryInfoPtr response_info) {
  telem_info_ = std::move(response_info);
}

void FakeProbeService::SetOemDataResponse(health::mojom::OemDataPtr oem_data) {
  oem_data_ = std::move(oem_data);
}

void FakeProbeService::BindPendingReceiver(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace ash
