// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_GEOLOCATION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_GEOLOCATION_SERVICE_ASH_H_

#include <string>

#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi GeolocationService interface. Lives in ash-chrome on
// the UI thread. Queries ash::GeolocationHandler for wifi access point
// data.
class GeolocationServiceAsh : public mojom::GeolocationService {
 public:
  GeolocationServiceAsh();
  GeolocationServiceAsh(const GeolocationServiceAsh&) = delete;
  GeolocationServiceAsh& operator=(const GeolocationServiceAsh&) = delete;
  ~GeolocationServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::GeolocationService> receiver);

  // crosapi::mojom::GeolocationService:
  void GetWifiAccessPoints(GetWifiAccessPointsCallback callback) override;
  void TrackGeolocationAttempted(const std::string& name) override;
  void TrackGeolocationRelinquished(const std::string& name) override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::GeolocationService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_GEOLOCATION_SERVICE_ASH_H_
