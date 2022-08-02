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

class FakeProbeService : public crosapi::mojom::ProbeService {
 public:
  class Factory : public ash::ProbeService::Factory {
   public:
    Factory();
    ~Factory() override;

    void SetCreateInstanceResponse(
        std::unique_ptr<FakeProbeService> fake_service);

   protected:
    // ProbeService::Factory:
    std::unique_ptr<crosapi::mojom::ProbeService> CreateInstance(
        mojo::PendingReceiver<crosapi::mojom::ProbeService> receiver) override;

   private:
    crosapi::mojom::ProbeTelemetryInfoPtr telem_info_{
        crosapi::mojom::ProbeTelemetryInfo::New()};

    crosapi::mojom::ProbeOemDataPtr oem_data_{
        crosapi::mojom::ProbeOemData::New()};

    std::vector<crosapi::mojom::ProbeCategoryEnum> requested_categories_;

   private:
    std::unique_ptr<FakeProbeService> fake_service_;
  };

  FakeProbeService();
  FakeProbeService(const FakeProbeService&) = delete;
  FakeProbeService& operator=(const FakeProbeService&) = delete;
  ~FakeProbeService() override;

  // crosapi::mojom::ProbeService overrides.
  void ProbeTelemetryInfo(
      const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;

  void GetOemData(GetOemDataCallback callback) override;

  // Sets the return value for |ProbeTelemetryInfo|.
  void SetProbeTelemetryInfoResponse(
      crosapi::mojom::ProbeTelemetryInfoPtr response_info);

  // Sets the return value for |GetOemData|.
  void SetOemDataResponse(crosapi::mojom::ProbeOemDataPtr oem_data);

  // Set expectation about the parameter that is passed to |ProbeTelemetryInfo|.
  void SetExpectedLastRequestedCategories(
      std::vector<crosapi::mojom::ProbeCategoryEnum>
          expected_requested_categories);

 private:
  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::ProbeService> receiver);

  mojo::Receiver<crosapi::mojom::ProbeService> receiver_;

  // Response for a call to |ProbeTelemetryInfo|.
  crosapi::mojom::ProbeTelemetryInfoPtr telem_info_{
      crosapi::mojom::ProbeTelemetryInfo::New()};

  // Response for a call to |GetOemData|.
  crosapi::mojom::ProbeOemDataPtr oem_data_{
      crosapi::mojom::ProbeOemData::New()};

  // Expectation about the parameter that is passed to |ProbeTelemetryInfo|.
  std::vector<crosapi::mojom::ProbeCategoryEnum> actual_requested_categories_;

  // Actual passed parameter.
  std::vector<crosapi::mojom::ProbeCategoryEnum> expected_requested_categories_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_H_
