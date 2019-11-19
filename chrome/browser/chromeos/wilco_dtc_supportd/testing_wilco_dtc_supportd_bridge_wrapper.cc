// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/testing_wilco_dtc_supportd_bridge_wrapper.h"

#include <unistd.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/fake_wilco_dtc_supportd_client.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

// Testing implementation of the WilcoDtcSupportdServiceFactory Mojo service
// that allows to stub out the GetService Mojo method and tie it with the
// testing implementation instead.
class TestingMojoWilcoDtcSupportdServiceFactory final
    : public wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory {
 public:
  // |get_service_handler_callback| is the callback that will be run when
  // GetService() is called.
  explicit TestingMojoWilcoDtcSupportdServiceFactory(
      base::RepeatingCallback<void(
          mojo::PendingReceiver<
              wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
              mojo_wilco_dtc_supportd_service_receiver,
          mojo::PendingRemote<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
              mojo_wilco_dtc_supportd_client)> get_service_handler_callback)
      : get_service_handler_callback_(std::move(get_service_handler_callback)) {
  }

  // Completes the Mojo receiver of |this| to the given Mojo pending receiver.
  // This method allows to redirect to |this| the calls on the
  // WilcoDtcSupportdServiceFactory interface that are made by the
  // WilcoDtcSupportdBridge.
  void Bind(mojo::PendingReceiver<
            wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>
                mojo_wilco_dtc_supportd_service_factory_receiver) {
    // First close the Mojo receiver in case it was previously completed, to
    // allow calling this method multiple times.
    self_receiver_.reset();
    self_receiver_.Bind(
        std::move(mojo_wilco_dtc_supportd_service_factory_receiver));
  }

  // WilcoDtcSupportdServiceFactory overrides:

  void GetService(
      mojo::PendingReceiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
          service,
      mojo::PendingRemote<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
          client,
      GetServiceCallback callback) override {
    DCHECK(service);
    DCHECK(client);
    // Redirect to |get_service_handler_callback_| to let
    // TestingWilcoDtcSupportdBridgeWrapper capture |client| (which points to
    // the production implementation in WilcoDtcSupportdBridge) and fulfill
    // |service| (to make it point to the stub implementation of the
    // WilcoDtcSupportdService Mojo service that was passed to
    // TestingWilcoDtcSupportdBridgeWrapper).
    get_service_handler_callback_.Run(std::move(service), std::move(client));
    std::move(callback).Run();
  }

 private:
  // Mojo receiver that binds |this| as an implementation of the
  // WilcoDtcSupportdClient Mojo interface.
  mojo::Receiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>
      self_receiver_{this};
  // The callback to be run when GetService() is called.
  base::RepeatingCallback<void(
      mojo::PendingReceiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
          mojo_wilco_dtc_supportd_service_receiver,
      mojo::PendingRemote<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
          mojo_wilco_dtc_supportd_client)>
      get_service_handler_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestingMojoWilcoDtcSupportdServiceFactory);
};

// Testing implementation of the WilcoDtcSupportdBridge delegate that stubs out
// the process of generating the Mojo invitation and tie it with
// TestingMojoWilcoDtcSupportdServiceFactory instead.
class TestingWilcoDtcSupportdBridgeWrapperDelegate final
    : public WilcoDtcSupportdBridge::Delegate {
 public:
  explicit TestingWilcoDtcSupportdBridgeWrapperDelegate(
      std::unique_ptr<TestingMojoWilcoDtcSupportdServiceFactory>
          mojo_wilco_dtc_supportd_service_factory)
      : mojo_wilco_dtc_supportd_service_factory_(
            std::move(mojo_wilco_dtc_supportd_service_factory)) {}

  // WilcoDtcSupportdBridge::Delegate overrides:

  void CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
      mojo::Remote<wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>*
          wilco_dtc_supportd_service_factory_mojo_remote,
      base::ScopedFD* remote_endpoint_fd) override {
    // Bind the Mojo pointer passed to the bridge with the
    // TestingMojoWilcoDtcSupportdServiceFactory implementation.
    mojo_wilco_dtc_supportd_service_factory_->Bind(
        wilco_dtc_supportd_service_factory_mojo_remote
            ->BindNewPipeAndPassReceiver());

    // Return a fake file descriptor - its value is not used in the unit test
    // environment for anything except comparing with zero.
    remote_endpoint_fd->reset(HANDLE_EINTR(dup(STDIN_FILENO)));
    DCHECK(remote_endpoint_fd->is_valid());
  }

 private:
  std::unique_ptr<TestingMojoWilcoDtcSupportdServiceFactory>
      mojo_wilco_dtc_supportd_service_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestingWilcoDtcSupportdBridgeWrapperDelegate);
};

FakeWilcoDtcSupportdClient* GetFakeDbusWilcoDtcSupportdClient() {
  WilcoDtcSupportdClient* const wilco_dtc_supportd_client =
      WilcoDtcSupportdClient::Get();
  DCHECK(wilco_dtc_supportd_client);
  return static_cast<FakeWilcoDtcSupportdClient*>(wilco_dtc_supportd_client);
}

}  // namespace

// static
std::unique_ptr<TestingWilcoDtcSupportdBridgeWrapper>
TestingWilcoDtcSupportdBridgeWrapper::Create(
    wilco_dtc_supportd::mojom::WilcoDtcSupportdService*
        mojo_wilco_dtc_supportd_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WilcoDtcSupportdBridge>* bridge) {
  return base::WrapUnique(new TestingWilcoDtcSupportdBridgeWrapper(
      mojo_wilco_dtc_supportd_service, std::move(url_loader_factory), bridge));
}

TestingWilcoDtcSupportdBridgeWrapper::~TestingWilcoDtcSupportdBridgeWrapper() =
    default;

void TestingWilcoDtcSupportdBridgeWrapper::EstablishFakeMojoConnection() {
  DCHECK(!mojo_wilco_dtc_supportd_client_);
  DCHECK(!mojo_get_service_handler_);

  // Set up the callback that will handle the GetService Mojo method called
  // during the bootstrap.
  base::RunLoop run_loop;
  mojo::PendingReceiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
      intercepted_mojo_wilco_dtc_supportd_service_receiver;
  mojo_get_service_handler_ = base::BindLambdaForTesting(
      [&run_loop, &intercepted_mojo_wilco_dtc_supportd_service_receiver](
          mojo::PendingReceiver<
              wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
              mojo_wilco_dtc_supportd_service_receiver) {
        intercepted_mojo_wilco_dtc_supportd_service_receiver =
            std::move(mojo_wilco_dtc_supportd_service_receiver);
        run_loop.Quit();
      });

  // Trigger the Mojo bootstrapping process by unblocking the corresponding
  // D-Bus operations.
  FakeWilcoDtcSupportdClient* const fake_dbus_wilco_dtc_supportd_client =
      GetFakeDbusWilcoDtcSupportdClient();
  fake_dbus_wilco_dtc_supportd_client->SetWaitForServiceToBeAvailableResult(
      true);
  fake_dbus_wilco_dtc_supportd_client->SetBootstrapMojoConnectionResult(true);

  // Wait till the GetService Mojo method call completes.
  run_loop.Run();
  DCHECK(mojo_wilco_dtc_supportd_client_);

  // First close the Mojo receiver in case it was previously completed, to allow
  // calling this method multiple times.
  mojo_wilco_dtc_supportd_service_receiver_.reset();
  mojo_wilco_dtc_supportd_service_receiver_.Bind(
      std::move(intercepted_mojo_wilco_dtc_supportd_service_receiver));
}

void TestingWilcoDtcSupportdBridgeWrapper::HandleMojoGetService(
    mojo::PendingReceiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
        mojo_wilco_dtc_supportd_service_receiver,
    mojo::PendingRemote<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
        mojo_wilco_dtc_supportd_client) {
  std::move(mojo_get_service_handler_)
      .Run(std::move(mojo_wilco_dtc_supportd_service_receiver));
  mojo_wilco_dtc_supportd_client_.Bind(
      std::move(mojo_wilco_dtc_supportd_client));
}

TestingWilcoDtcSupportdBridgeWrapper::TestingWilcoDtcSupportdBridgeWrapper(
    wilco_dtc_supportd::mojom::WilcoDtcSupportdService*
        mojo_wilco_dtc_supportd_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WilcoDtcSupportdBridge>* bridge)
    : mojo_wilco_dtc_supportd_service_receiver_(
          mojo_wilco_dtc_supportd_service) {
  auto profile_manager = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  CHECK(profile_manager->SetUp());
  *bridge = std::make_unique<WilcoDtcSupportdBridge>(
      std::make_unique<TestingWilcoDtcSupportdBridgeWrapperDelegate>(
          std::make_unique<TestingMojoWilcoDtcSupportdServiceFactory>(
              base::BindRepeating(
                  &TestingWilcoDtcSupportdBridgeWrapper::HandleMojoGetService,
                  base::Unretained(this)))),
      url_loader_factory,
      std::make_unique<WilcoDtcSupportdNotificationController>(
          profile_manager->profile_manager()));
}

}  // namespace chromeos
