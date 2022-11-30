// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/network_change_ash.h"
#include "chrome/browser/ash/network_change_manager_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "content/public/test/browser_test.h"

namespace crosapi {
namespace {

class MockNerworkChangeObserver : public mojom::NetworkChangeObserver {
 public:
  MOCK_METHOD(void,
              OnNetworkChanged,
              (bool dns_changed,
               bool ip_address_changed,
               bool connection_type_changed,
               mojom::ConnectionType new_connection_type,
               bool connection_subtype_changed,
               mojom::ConnectionSubtype new_connection_subtype),
              (override));
};

class NetworkChangeCrosapiTest : public InProcessBrowserTest {
 protected:
  testing::NiceMock<MockNerworkChangeObserver> observer;
  mojo::Receiver<mojom::NetworkChangeObserver> receiver{&observer};
};

IN_PROC_BROWSER_TEST_F(NetworkChangeCrosapiTest, OnNetworkChanged) {
  base::RunLoop run_loop;

  // When AddObserver() is called, OnNetworkChange() should also be called to
  // initialize network setup.
  EXPECT_CALL(observer,
              OnNetworkChanged(
                  /*dns_changed=*/false, /*ip_address_changed=*/false,
                  /*connection_type_changed=*/true,
                  mojom::ConnectionType(
                      net::NetworkChangeNotifier::CONNECTION_ETHERNET),
                  /*connection_subtype_changed=*/true,
                  mojom::ConnectionSubtype(
                      net::NetworkChangeNotifier::SUBTYPE_UNKNOWN)))
      .WillOnce([&] { run_loop.Quit(); });
  CrosapiManager::Get()->crosapi_ash()->network_change_ash()->AddObserver(
      receiver.BindNewPipeAndPassRemote());
  run_loop.Run();

  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));

  // When the network connection changes, OnNetworkChange() should be called.
  // TODO(crbug.com/1356920): Replace SuspendDone() by test crosapi at the time
  // we implement isolation test.
  EXPECT_CALL(
      observer,
      OnNetworkChanged(
          /*dns_changed=*/false, /*ip_address_changed=*/true,
          /*connection_type_changed=*/false,
          mojom::ConnectionType(net::NetworkChangeNotifier::CONNECTION_UNKNOWN),
          /*connection_subtype_changed=*/false,
          mojom::ConnectionSubtype(net::NetworkChangeNotifier::SUBTYPE_NONE)));
  ash::NetworkChangeManagerClient::GetInstance()->SuspendDone(
      base::TimeDelta());
}

}  // namespace
}  // namespace crosapi
