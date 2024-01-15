// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/tcp_socket/nearby_connections_tcp_socket_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint32_t kBacklog = 10;
const net::MutableNetworkTrafficAnnotationTag kAnnotation =
    net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
const net::IPEndPoint kLocalAddress(net::IPAddress(192, 168, 86, 01),
                                    ash::nearby::TcpServerSocketPort::kMin);
const net::IPEndPoint kRemoteAddress(net::IPAddress(192, 168, 86, 02),
                                     ash::nearby::TcpServerSocketPort::kMax);

}  // namespace

class NearbyConnectionsTcpSocketFactoryTest : public ::testing::Test {
 protected:
  // Verifies input data and immediately invokes callbacks.
  class FakeNetworkContext : public network::TestNetworkContext {
   public:
    FakeNetworkContext() = default;
    ~FakeNetworkContext() override = default;

    bool should_invoke_connect_callback_ = true;

   private:
    // network::TestNetworkContext:
    void CreateTCPServerSocket(
        const net::IPEndPoint& local_addr,
        network::mojom::TCPServerSocketOptionsPtr options,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
        mojo::PendingReceiver<network::mojom::TCPServerSocket> socket,
        CreateTCPServerSocketCallback callback) override {
      EXPECT_EQ(kLocalAddress, local_addr);
      EXPECT_EQ(kBacklog, options->backlog);
      EXPECT_EQ(traffic_annotation, net::MutableNetworkTrafficAnnotationTag(
                                        TRAFFIC_ANNOTATION_FOR_TESTS));
      std::move(callback).Run(net::OK, local_addr);
    }
    void CreateTCPConnectedSocket(
        const std::optional<net::IPEndPoint>& local_addr,
        const net::AddressList& remote_addr_list,
        network::mojom::TCPConnectedSocketOptionsPtr
            tcp_connected_socket_options,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
        mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
        mojo::PendingRemote<network::mojom::SocketObserver> observer,
        CreateTCPConnectedSocketCallback callback) override {
      EXPECT_EQ(kLocalAddress, local_addr);
      EXPECT_EQ(1u, remote_addr_list.size());
      EXPECT_EQ(kRemoteAddress, remote_addr_list[0]);
      EXPECT_EQ(kAnnotation, traffic_annotation);
      if (should_invoke_connect_callback_) {
        std::move(callback).Run(
            net::OK, local_addr, remote_addr_list[0],
            /*receive_stream=*/mojo::ScopedDataPipeConsumerHandle(),
            /*send_stream=*/mojo::ScopedDataPipeProducerHandle());
      }
    }
  };

  NearbyConnectionsTcpSocketFactoryTest() = default;

  ~NearbyConnectionsTcpSocketFactoryTest() override = default;

  void SetUp() override {
    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    factory_ =
        std::make_unique<NearbyConnectionsTcpSocketFactory>(base::BindRepeating(
            &NearbyConnectionsTcpSocketFactoryTest::GetNetworkContext,
            base::Unretained(this)));
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return fake_network_context_.get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  std::unique_ptr<NearbyConnectionsTcpSocketFactory> factory_;
};

TEST_F(NearbyConnectionsTcpSocketFactoryTest, NetworkContextExists) {
  {
    base::RunLoop run_loop;
    factory_->CreateTCPServerSocket(
        kLocalAddress.address(),
        *ash::nearby::TcpServerSocketPort::FromUInt16(kLocalAddress.port()),
        kBacklog, kAnnotation, /*receiver=*/mojo::NullReceiver(),
        base::BindLambdaForTesting(
            [&run_loop](int32_t result,
                        const std::optional<net::IPEndPoint>& local_addr) {
              EXPECT_EQ(net::OK, result);
              EXPECT_EQ(kLocalAddress, local_addr);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    factory_->CreateTCPConnectedSocket(
        /*timeout=*/base::Seconds(5), kLocalAddress,
        net::AddressList(kRemoteAddress),
        /*tcp_connected_socket_options=*/nullptr, kAnnotation,
        /*receiver=*/mojo::NullReceiver(),
        /*observer=*/mojo::NullRemote(),
        base::BindLambdaForTesting(
            [&run_loop](int32_t result,
                        const std::optional<net::IPEndPoint>& local_addr,
                        const std::optional<net::IPEndPoint>& peer_addr,
                        mojo::ScopedDataPipeConsumerHandle receive_stream,
                        mojo::ScopedDataPipeProducerHandle send_stream) {
              EXPECT_EQ(net::OK, result);
              EXPECT_EQ(kLocalAddress, local_addr);
              EXPECT_EQ(kRemoteAddress, peer_addr);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(NearbyConnectionsTcpSocketFactoryTest, NetworkContextDoesNotExist) {
  fake_network_context_.reset();

  // Expect trivial data in callback when the network context is null.
  {
    base::RunLoop run_loop;
    factory_->CreateTCPServerSocket(
        kLocalAddress.address(),
        *ash::nearby::TcpServerSocketPort::FromUInt16(kLocalAddress.port()),
        kBacklog, kAnnotation, /*receiver=*/mojo::NullReceiver(),
        base::BindLambdaForTesting(
            [&run_loop](int32_t result,
                        const std::optional<net::IPEndPoint>& local_addr) {
              EXPECT_EQ(net::ERR_FAILED, result);
              EXPECT_EQ(std::nullopt, local_addr);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    factory_->CreateTCPConnectedSocket(
        /*timeout=*/base::Seconds(5), kLocalAddress,
        net::AddressList(kRemoteAddress),
        /*tcp_connected_socket_options=*/nullptr, kAnnotation,
        /*receiver=*/mojo::NullReceiver(),
        /*observer=*/mojo::NullRemote(),
        base::BindLambdaForTesting(
            [&run_loop](int32_t result,
                        const std::optional<net::IPEndPoint>& local_addr,
                        const std::optional<net::IPEndPoint>& peer_addr,
                        mojo::ScopedDataPipeConsumerHandle receive_stream,
                        mojo::ScopedDataPipeProducerHandle send_stream) {
              EXPECT_EQ(net::ERR_FAILED, result);
              EXPECT_EQ(std::nullopt, local_addr);
              EXPECT_EQ(std::nullopt, peer_addr);
              EXPECT_EQ(mojo::ScopedDataPipeConsumerHandle(), receive_stream);
              EXPECT_EQ(mojo::ScopedDataPipeProducerHandle(), send_stream);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(NearbyConnectionsTcpSocketFactoryTest, ConnectTimeout) {
  // Finishes in time.
  {
    base::RunLoop run_loop;
    factory_->CreateTCPConnectedSocket(
        /*timeout=*/base::Seconds(5), kLocalAddress,
        net::AddressList(kRemoteAddress),
        /*tcp_connected_socket_options=*/nullptr, kAnnotation,
        /*receiver=*/mojo::NullReceiver(),
        /*observer=*/mojo::NullRemote(),
        base::BindLambdaForTesting(
            [&run_loop](int32_t result,
                        const std::optional<net::IPEndPoint>& local_addr,
                        const std::optional<net::IPEndPoint>& peer_addr,
                        mojo::ScopedDataPipeConsumerHandle receive_stream,
                        mojo::ScopedDataPipeProducerHandle send_stream) {
              EXPECT_EQ(net::OK, result);
              EXPECT_EQ(kLocalAddress, local_addr);
              EXPECT_EQ(kRemoteAddress, peer_addr);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Does not finish in time.
  {
    base::RunLoop run_loop;

    // Make NetworkContext never finish.
    fake_network_context_->should_invoke_connect_callback_ = false;

    factory_->CreateTCPConnectedSocket(
        /*timeout=*/base::Seconds(5), kLocalAddress,
        net::AddressList(kRemoteAddress),
        /*tcp_connected_socket_options=*/nullptr, kAnnotation,
        /*receiver=*/mojo::NullReceiver(),
        /*observer=*/mojo::NullRemote(),
        base::BindLambdaForTesting(
            [&run_loop](int32_t result,
                        const std::optional<net::IPEndPoint>& local_addr,
                        const std::optional<net::IPEndPoint>& peer_addr,
                        mojo::ScopedDataPipeConsumerHandle receive_stream,
                        mojo::ScopedDataPipeProducerHandle send_stream) {
              EXPECT_EQ(net::ERR_TIMED_OUT, result);
              EXPECT_EQ(std::nullopt, local_addr);
              EXPECT_EQ(std::nullopt, peer_addr);
              EXPECT_EQ(mojo::ScopedDataPipeConsumerHandle(), receive_stream);
              EXPECT_EQ(mojo::ScopedDataPipeProducerHandle(), send_stream);
              run_loop.Quit();
            }));
    // Note: We do not need to call TaskEnvironment::FastForwardBy();
    // RunLoop::Run() auto-advances to the soonest delayed task when all managed
    // threads are idle.
    run_loop.Run();
  }
}
