// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GEOLOCATION_TEST_GEOLOCATION_URL_LOADER_FACTORY_H_
#define ASH_SYSTEM_GEOLOCATION_TEST_GEOLOCATION_URL_LOADER_FACTORY_H_

#include "chromeos/ash/components/geolocation/geoposition.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace ash {

// A fake SharedURLLoaderFactory that always responds with 200 OK status with a
// `geoposition_` chosen by the test client.
class TestGeolocationUrlLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  TestGeolocationUrlLoaderFactory();

  TestGeolocationUrlLoaderFactory(const TestGeolocationUrlLoaderFactory&) =
      delete;
  TestGeolocationUrlLoaderFactory& operator=(
      const TestGeolocationUrlLoaderFactory&) = delete;

  // network::SharedURLLoaderFactory
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

  void set_position(Geoposition position) { position_ = position; }
  const Geoposition& position() const { return position_; }

  void SetValidPosition(double latitude,
                        double longitude,
                        base::Time timestamp);

  // Clears all added responses in `test_url_loader_factory_`.
  void ClearResponses();

 protected:
  ~TestGeolocationUrlLoaderFactory() override;

 private:
  // Used to control a server response data corresponding to a request url.
  network::TestURLLoaderFactory test_url_loader_factory_;

  // The geoposition to be responded from this factory when a client makes a
  // request to any url.
  Geoposition position_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_GEOLOCATION_TEST_GEOLOCATION_URL_LOADER_FACTORY_H_
