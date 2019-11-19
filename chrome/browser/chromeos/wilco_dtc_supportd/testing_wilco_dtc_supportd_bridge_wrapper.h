// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_TESTING_WILCO_DTC_SUPPORTD_BRIDGE_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_TESTING_WILCO_DTC_SUPPORTD_BRIDGE_WRAPPER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_bridge.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

// Manages a fake instance of WilcoDtcSupportdBridge for unit tests. Mocks out
// the Mojo communication and provides tools for simulating and handling Mojo
// requests.
class TestingWilcoDtcSupportdBridgeWrapper final {
 public:
  // |mojo_wilco_dtc_supportd_service| is an unowned pointer that should be a
  // stub implementation of the WilcoDtcSupportdService Mojo service (which in
  // production is implemented by the wilco_dtc_supportd daemon). |bridge| is an
  // unowned bridge instance that holds the stub wilco_dtc_supportd bridge
  // instance created by the TestingWilcoDtcSupportdBridgeWrapper.
  static std::unique_ptr<TestingWilcoDtcSupportdBridgeWrapper> Create(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdService*
          mojo_wilco_dtc_supportd_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loade_factory,
      std::unique_ptr<WilcoDtcSupportdBridge>* bridge);

  ~TestingWilcoDtcSupportdBridgeWrapper();

  // Simulates bootstrapping the Mojo communication between the
  // wilco_dtc_supportd daemon and the browser.
  void EstablishFakeMojoConnection();

  // Returns a pointer that allows to simulate Mojo calls to the
  // WilcoDtcSupportdClient mojo service (which in production is implemented by
  // the browser and called by the wilco_dtc_supportd daemon).
  //
  // Returns null if EstablishFakeMojoConnection() wasn't called yet.
  wilco_dtc_supportd::mojom::WilcoDtcSupportdClient*
  mojo_wilco_dtc_supportd_client() {
    return mojo_wilco_dtc_supportd_client_.get();
  }

 private:
  // |mojo_wilco_dtc_supportd_service| is an unowned pointer that should be a
  // stub implementation of the WilcoDtcSupportdService Mojo service (which in
  // production is implemented by the wilco_dtc_supportd daemon).
  TestingWilcoDtcSupportdBridgeWrapper(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdService*
          mojo_wilco_dtc_supportd_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<WilcoDtcSupportdBridge>* bridge);

  // Implements the GetService Mojo method of the WilcoDtcSupportdServiceFactory
  // interface. Called during the simulated Mojo boostrapping.
  void HandleMojoGetService(
      mojo::PendingReceiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
          mojo_wilco_dtc_supportd_service_receiver,
      mojo::PendingRemote<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
          mojo_wilco_dtc_supportd_client);

  // Mojo receiver that binds the WilcoDtcSupportdService implementation (passed
  // to the constructor) with the other endpoint owned from |bridge_|.
  mojo::Receiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
      mojo_wilco_dtc_supportd_service_receiver_;

  // Mojo pointer that points to the WilcoDtcSupportdClient implementation
  // (owned by |bridge_|).  Is initialized if the Mojo is bootstrapped by
  // EstablishFakeMojoConnection().
  mojo::Remote<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
      mojo_wilco_dtc_supportd_client_;

  // Temporary callback that allows to deliver the
  // WilcoDtcSupportdServiceRequest value during the Mojo bootstrapping
  // simulation by EstablishFakeMojoConnection().
  base::OnceCallback<void(
      mojo::PendingReceiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
          mojo_wilco_dtc_supportd_service_receiver)>
      mojo_get_service_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestingWilcoDtcSupportdBridgeWrapper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_TESTING_WILCO_DTC_SUPPORTD_BRIDGE_WRAPPER_H_
