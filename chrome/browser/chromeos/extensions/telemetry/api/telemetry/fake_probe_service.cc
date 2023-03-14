// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/fake_probe_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

FakeProbeService::FakeProbeService() : receiver_(this) {}

FakeProbeService::~FakeProbeService() {
  // Assert on the expectations.
  EXPECT_EQ(actual_requested_categories_, expected_requested_categories_);
}

void FakeProbeService::BindPendingReceiver(
    mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeProbeService::ProbeTelemetryInfo(
    const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  actual_requested_categories_.clear();
  actual_requested_categories_.insert(actual_requested_categories_.end(),
                                      categories.begin(), categories.end());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), telem_info_.Clone()));
}

void FakeProbeService::GetOemData(GetOemDataCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), oem_data_.Clone()));
}

void FakeProbeService::SetExpectedLastRequestedCategories(
    std::vector<crosapi::mojom::ProbeCategoryEnum>
        expected_requested_categories) {
  expected_requested_categories_ = std::move(expected_requested_categories);
}

void FakeProbeService::SetProbeTelemetryInfoResponse(
    crosapi::mojom::ProbeTelemetryInfoPtr response_info) {
  telem_info_ = std::move(response_info);
}

void FakeProbeService::SetOemDataResponse(
    crosapi::mojom::ProbeOemDataPtr oem_data) {
  oem_data_ = std::move(oem_data);
}

}  // namespace chromeos
