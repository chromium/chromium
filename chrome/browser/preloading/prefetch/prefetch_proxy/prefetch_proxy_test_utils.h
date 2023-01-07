// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_TEST_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_TEST_UTILS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace net {
class TCPClientSocket;
class StreamSocket;
class IOBuffer;
class DrainableIOBuffer;
}  // namespace net

// Pumps data from one socket to another.
class TestProxySocketDataPump {
 public:
  TestProxySocketDataPump(net::StreamSocket* from_socket,
                          net::StreamSocket* to_socket,
                          base::OnceClosure on_done_callback);

  TestProxySocketDataPump(const TestProxySocketDataPump&) = delete;
  TestProxySocketDataPump& operator=(const TestProxySocketDataPump&) = delete;

  ~TestProxySocketDataPump();

  void Start();

 private:
  void Read();
  void HandleReadResult(int result);

  void Write();
  void HandleWriteResult(int result);

  raw_ptr<net::StreamSocket, DanglingUntriaged> from_socket_;
  raw_ptr<net::StreamSocket, DanglingUntriaged> to_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  base::OnceClosure on_done_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Represents a single CONNECT proxy tunnel connection used for browser tests.
// This class should be owned and called by a single IO thread.
class TestProxyTunnelConnection {
 public:
  TestProxyTunnelConnection();

  TestProxyTunnelConnection(const TestProxyTunnelConnection&) = delete;
  TestProxyTunnelConnection& operator=(const TestProxyTunnelConnection&) =
      delete;

  ~TestProxyTunnelConnection();

  base::WeakPtr<TestProxyTunnelConnection> GetWeakPtr();

  // Returns true if |ConnectToPeerOnLocalhost| has already been completed
  // successfully but |StartProxy| hasn't been called yet.
  bool IsReadyForIncomingSocket() const;

  // This should be called when a new proxy connection should be opened to
  // |port|. Returns true on success.
  bool ConnectToPeerOnLocalhost(int port);

  // Starts proxying the given socket to/from the socket opened in
  // |ConnectToPeerOnLocalhost|.
  void StartProxy(std::unique_ptr<net::StreamSocket> incoming_socket);

  // Runs |on_done_callback| when the owned sockets close, either from EOF or an
  // error. It is safe to delete |this| during the callback.
  void SetOnDoneCallback(base::OnceClosure on_done_callback);

 private:
  void OnDone();

  void HandleConnectResult(int result);

  // Ran when |OnDone()| is called. It is safe to delete |this| during this
  // callback.
  base::OnceClosure on_done_callback_;

  // Used to signal when |client_socket_| is done connecting.
  base::OnceClosure wait_for_connect_closure_;

  std::unique_ptr<net::TCPClientSocket> client_socket_;
  std::unique_ptr<net::StreamSocket> incoming_socket_;
  std::unique_ptr<TestProxySocketDataPump> incoming_pump_;
  std::unique_ptr<TestProxySocketDataPump> outgoing_pump_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TestProxyTunnelConnection> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_TEST_UTILS_H_
