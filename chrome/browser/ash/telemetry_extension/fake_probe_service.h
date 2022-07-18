// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_H_

#include <memory>
#include <vector>

#include "chrome/browser/ash/telemetry_extension/probe_service.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class FakeProbeService : public health::mojom::ProbeService {
 public:
  class Factory : public ash::ProbeService::Factory {
   public:
    Factory();
    ~Factory() override;

    void SetCreateInstanceResponse(
        std::unique_ptr<FakeProbeService> fake_service);

   protected:
    // ProbeService::Factory:
    std::unique_ptr<health::mojom::ProbeService> CreateInstance(
        mojo::PendingReceiver<health::mojom::ProbeService> receiver) override;

   private:
    health::mojom::TelemetryInfoPtr telem_info_{
        health::mojom::TelemetryInfo::New()};

    health::mojom::OemDataPtr oem_data_{health::mojom::OemData::New()};

    std::vector<health::mojom::ProbeCategoryEnum> requested_categories_;

   private:
    std::unique_ptr<FakeProbeService> fake_service_;
  };

  FakeProbeService();
  FakeProbeService(const FakeProbeService&) = delete;
  FakeProbeService& operator=(const FakeProbeService&) = delete;
  ~FakeProbeService() override;

  // health::mojom::ProbeService overrides.
  void ProbeTelemetryInfo(
      const std::vector<health::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;

  void GetOemData(GetOemDataCallback callback) override;

  // Sets the return value for |ProbeTelemetryInfo|.
  void SetProbeTelemetryInfoResponse(
      health::mojom::TelemetryInfoPtr response_info);

  // Sets the return value for |GetOemData|.
  void SetOemDataResponse(health::mojom::OemDataPtr oem_data);

  // Set expectation about the parameter that is passed to |ProbeTelemetryInfo|.
  void SetExpectedLastRequestedCategories(
      std::vector<health::mojom::ProbeCategoryEnum>
          expected_requested_categories);

 private:
  void BindPendingReceiver(
      mojo::PendingReceiver<health::mojom::ProbeService> receiver);

  mojo::Receiver<health::mojom::ProbeService> receiver_;

  // Response for a call to |ProbeTelemetryInfo|.
  health::mojom::TelemetryInfoPtr telem_info_{
      health::mojom::TelemetryInfo::New()};

  // Response for a call to |GetOemData|.
  health::mojom::OemDataPtr oem_data_{health::mojom::OemData::New()};

  // Expectation about the parameter that is passed to |ProbeTelemetryInfo|.
  std::vector<health::mojom::ProbeCategoryEnum> actual_requested_categories_;

  // Actual passed parameter.
  std::vector<health::mojom::ProbeCategoryEnum> expected_requested_categories_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_H_
