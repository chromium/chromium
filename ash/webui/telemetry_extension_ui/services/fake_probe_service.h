// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_FAKE_PROBE_SERVICE_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_FAKE_PROBE_SERVICE_H_

#include <memory>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom-shared.h"
#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/probe_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class FakeProbeService : public health::mojom::ProbeService {
 public:
  class Factory : public ash::ProbeService::Factory {
   public:
    Factory();
    ~Factory() override;

    // Sets the one-time response of |ProbeTelemetryInfo| method.
    void SetProbeTelemetryInfoResponseForTesting(
        health::mojom::TelemetryInfoPtr response_info);

    // Sets the one-time response of |GetOemData| method.
    void SetOemDataResponseForTesting(health::mojom::OemDataPtr oem_data);

    // Returns all categories that have been requested after the
    // FakeProbeService has been created and invoked.
    std::vector<health::mojom::ProbeCategoryEnum>
    GetAndClearRequestedCategories();

   protected:
    // ProbeService::Factory:
    std::unique_ptr<health::mojom::ProbeService> CreateInstance(
        mojo::PendingReceiver<health::mojom::ProbeService> receiver) override;

   private:
    health::mojom::TelemetryInfoPtr telem_info_{
        health::mojom::TelemetryInfo::New()};

    health::mojom::OemDataPtr oem_data_{health::mojom::OemData::New()};

    std::vector<health::mojom::ProbeCategoryEnum> requested_categories_;
  };

  FakeProbeService(const FakeProbeService&) = delete;
  FakeProbeService& operator=(const FakeProbeService&) = delete;
  ~FakeProbeService() override;

 private:
  explicit FakeProbeService(
      mojo::PendingReceiver<health::mojom::ProbeService> receiver,
      health::mojom::TelemetryInfoPtr telem_info,
      health::mojom::OemDataPtr oem_data,
      std::vector<health::mojom::ProbeCategoryEnum>* requested_categories);

  void ProbeTelemetryInfo(
      const std::vector<health::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;

  void GetOemData(GetOemDataCallback callback) override;

  mojo::Receiver<health::mojom::ProbeService> receiver_;

  health::mojom::TelemetryInfoPtr telem_info_;

  health::mojom::OemDataPtr oem_data_;

  // A pointer to the requested categories, gets filled up by
  // |ProbeTelemetryInfo|.
  std::vector<health::mojom::ProbeCategoryEnum>* requested_categories_;
};

}  // namespace ash

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_FAKE_PROBE_SERVICE_H_
