// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_FAKE_PROBE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_FAKE_PROBE_SERVICE_H_

#include <memory>
#include <vector>

#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class FakeProbeService : public crosapi::mojom::TelemetryProbeService {
 public:
  FakeProbeService();
  FakeProbeService(const FakeProbeService&) = delete;
  FakeProbeService& operator=(const FakeProbeService&) = delete;
  ~FakeProbeService() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver);

  mojo::PendingRemote<crosapi::mojom::TelemetryProbeService>
  BindNewPipeAndPassRemote();

  // crosapi::mojom::TelemetryProbeService overrides.
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
  mojo::Receiver<crosapi::mojom::TelemetryProbeService> receiver_;

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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_FAKE_PROBE_SERVICE_H_
