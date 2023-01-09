// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_test_utils.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {
const int kBufferSize = 1024;
}

TestProxySocketDataPump::TestProxySocketDataPump(
    net::StreamSocket* from_socket,
    net::StreamSocket* to_socket,
    base::OnceClosure on_done_callback)
    : from_socket_(from_socket),
      to_socket_(to_socket),
      on_done_callback_(std::move(on_done_callback)) {
  read_buffer_ = base::MakeRefCounted<net::IOBuffer>(kBufferSize);
}

TestProxySocketDataPump::~TestProxySocketDataPump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TestProxySocketDataPump::Start() {
  Read();
}

void TestProxySocketDataPump::Read() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!write_buffer_);

  int result = from_socket_->Read(
      read_buffer_.get(), kBufferSize,
      base::BindOnce(&TestProxySocketDataPump::HandleReadResult,
                     base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    HandleReadResult(result);
}

void TestProxySocketDataPump::HandleReadResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result <= 0) {
    std::move(on_done_callback_).Run();
    return;
  }

  write_buffer_ =
      base::MakeRefCounted<net::DrainableIOBuffer>(read_buffer_, result);
  Write();
}

void TestProxySocketDataPump::Write() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(write_buffer_);

  int result = to_socket_->Write(
      write_buffer_.get(), write_buffer_->BytesRemaining(),
      base::BindOnce(&TestProxySocketDataPump::HandleWriteResult,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  if (result != net::ERR_IO_PENDING)
    HandleWriteResult(result);
}

void TestProxySocketDataPump::HandleWriteResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result <= 0) {
    std::move(on_done_callback_).Run();
    return;
  }

  write_buffer_->DidConsume(result);
  if (write_buffer_->BytesRemaining()) {
    Write();
  } else {
    write_buffer_ = nullptr;
    Read();
  }
}

TestProxyTunnelConnection::TestProxyTunnelConnection() = default;
TestProxyTunnelConnection::~TestProxyTunnelConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::WeakPtr<TestProxyTunnelConnection>
TestProxyTunnelConnection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool TestProxyTunnelConnection::IsReadyForIncomingSocket() const {
  return !!client_socket_ && !incoming_socket_;
}

bool TestProxyTunnelConnection::ConnectToPeerOnLocalhost(int port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_socket_ = std::make_unique<net::TCPClientSocket>(
      net::AddressList(net::IPEndPoint(net::IPAddress::IPv4Localhost(), port)),
      nullptr, nullptr, nullptr, net::NetLogSource());

  int result = client_socket_->Connect(base::BindOnce(
      &TestProxyTunnelConnection::HandleConnectResult, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    HandleConnectResult(result);
  } else {
    base::RunLoop run_loop;
    wait_for_connect_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  return !!client_socket_;
}

void TestProxyTunnelConnection::HandleConnectResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result < 0) {
    LOG(ERROR) << "Connection failed: " << net::ErrorToString(result);
    client_socket_.reset();
  }
  if (wait_for_connect_closure_) {
    std::move(wait_for_connect_closure_).Run();
  }
}

void TestProxyTunnelConnection::StartProxy(
    std::unique_ptr<net::StreamSocket> incoming_socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  incoming_socket_ = std::move(incoming_socket);

  incoming_pump_ = std::make_unique<TestProxySocketDataPump>(
      client_socket_.get(), incoming_socket_.get(),
      base::BindOnce(&TestProxyTunnelConnection::OnDone,
                     base::Unretained(this)));
  outgoing_pump_ = std::make_unique<TestProxySocketDataPump>(
      incoming_socket_.get(), client_socket_.get(),
      base::BindOnce(&TestProxyTunnelConnection::OnDone,
                     base::Unretained(this)));

  incoming_pump_->Start();
  outgoing_pump_->Start();
}

void TestProxyTunnelConnection::SetOnDoneCallback(
    base::OnceClosure on_done_callback) {
  on_done_callback_ = std::move(on_done_callback);
}

void TestProxyTunnelConnection::OnDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_socket_.reset();
  incoming_socket_ = nullptr;

  if (on_done_callback_) {
    std::move(on_done_callback_).Run();
    // |this| may have been deleted by the callback, don't do anything else.
    return;
  }
}
