// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/geolocation_service_ash.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "ui/base/clipboard/clipboard.h"

namespace crosapi {

namespace {

void DoWifiScanTaskOnNetworkHandlerThread(
    GeolocationServiceAsh::GetWifiAccessPointsCallback callback) {
  DCHECK(ash::NetworkHandler::Get()->task_runner()->BelongsToCurrentThread());
  std::vector<mojom::AccessPointDataPtr> ap_data_vector;

  // If wifi isn't enabled, we've effectively completed the task.
  ash::GeolocationHandler* const geolocation_handler =
      ash::NetworkHandler::Get()->geolocation_handler();
  if (!geolocation_handler || !geolocation_handler->wifi_enabled()) {
    std::move(callback).Run(true, true, base::TimeDelta(),
                            std::move(ap_data_vector));
    return;
  }

  ash::WifiAccessPointVector access_points;
  int64_t age_ms = 0;
  if (!geolocation_handler->GetWifiAccessPoints(&access_points, &age_ms)) {
    std::move(callback).Run(true, false, base::TimeDelta(),
                            std::move(ap_data_vector));
    return;
  }

  for (const auto& access_point : access_points) {
    auto ap_data = mojom::AccessPointData::New();
    ap_data->mac_address = base::ASCIIToUTF16(access_point.mac_address);
    ap_data->radio_signal_strength = access_point.signal_strength;
    ap_data->channel = access_point.channel;
    ap_data->signal_to_noise = access_point.signal_to_noise;
    ap_data->ssid = base::UTF8ToUTF16(access_point.ssid);
    ap_data_vector.push_back(std::move(ap_data));
  }
  std::move(callback).Run(true, true, base::Milliseconds(age_ms),
                          std::move(ap_data_vector));
}

}  // namespace

GeolocationServiceAsh::GeolocationServiceAsh() = default;

GeolocationServiceAsh::~GeolocationServiceAsh() = default;

void GeolocationServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::GeolocationService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void GeolocationServiceAsh::GetWifiAccessPoints(
    GetWifiAccessPointsCallback callback) {
  // If in startup or shutdown, NetworkHandler is uninitialized.
  if (!ash::NetworkHandler::IsInitialized()) {
    std::move(callback).Run(false, false, base::TimeDelta(),
                            std::vector<mojom::AccessPointDataPtr>());
    return;
  }

  // We should return the response on current thread (mojo thread).
  auto callback_on_current_thread =
      base::BindPostTaskToCurrentDefault(std::move(callback), FROM_HERE);

  ash::NetworkHandler::Get()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DoWifiScanTaskOnNetworkHandlerThread,
                                std::move(callback_on_current_thread)));
}

void GeolocationServiceAsh::TrackGeolocationAttempted(const std::string& name) {
  ash::privacy_hub_util::TrackGeolocationAttempted(name);
}

void GeolocationServiceAsh::TrackGeolocationRelinquished(
    const std::string& name) {
  ash::privacy_hub_util::TrackGeolocationRelinquished(name);
}

}  // namespace crosapi
