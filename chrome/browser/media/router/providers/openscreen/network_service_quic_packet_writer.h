// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_NETWORK_SERVICE_QUIC_PACKET_WRITER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_NETWORK_SERVICE_QUIC_PACKET_WRITER_H_

#include <stddef.h>
#include <memory>

#include "base/macros.h"

#include "mojo/public/cpp/bindings/binding.h"

#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"

#include "chrome/browser/media/router/providers/openscreen/network_service_async_packet_sender.h"

namespace media_router {

// Chrome-specific packet writer. Intended for use outside of the Network
// service, this class uses the network service's UdpSocket for sending and
// receiving packets.
//
// TaskRunner usage:
// This class depends on a provided SingleThreadTaskRunner to provide thread
// safety for internal class members. Public QuicPacketWriter method overrides
// may be called on any thread, however most method calls on threads other than
// the TaskRunner's thread result in a task enqueued to the TaskRunner. Some
// trivial methods such as IsWriteBlocked, IsBatchMode, run synchronously and do
// not enqueue tasks.
class NetworkServiceQuicPacketWriter : quic::QuicPacketWriter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when we received an async reply from the socket that a write
    // error occurred.
    virtual void OnWriteError(net::Error error) = 0;

    // Called when our write status has changed from blocked to unblocked.
    virtual void OnWriteUnblocked() = 0;
  };

  explicit NetworkServiceQuicPacketWriter(
      std::unique_ptr<AsyncPacketSender> async_packet_sender,
      Delegate* delegate,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  ~NetworkServiceQuicPacketWriter() override;

  // quic::QuicPacketWriter
  quic::WriteResult Flush() override;

  bool IsBatchMode() const override;
  bool IsWriteBlocked() const override;

  quic::QuicByteCount GetMaxPacketSize(
      const quic::QuicSocketAddress& peer_address) const override;
  char* GetNextWriteLocation(
      const quic::QuicIpAddress& self_address,
      const quic::QuicSocketAddress& peer_address) override;

  void SetWritable() override;
  bool SupportsReleaseTime() const override;

  quic::WriteResult WritePacket(const char* buffer,
                                size_t buf_len,
                                const quic::QuicIpAddress& self_address,
                                const quic::QuicSocketAddress& peer_address,
                                quic::PerPacketOptions* options) override;

  // Test only methods
  // Need to set MaxPacketsInFlight for testing, since the real world default
  // causes issues with gmock's StrictMock WillRepeatedly method due to being
  // too high.
  void SetMaxPacketsInFlightForTesting(std::size_t max_packets) {
    max_packets_in_flight_ = max_packets;
  }

 private:
  template <typename Functor, typename... Args>
  void EnqueueTask(Functor f, Args&&... args) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(f, weak_factory_.GetWeakPtr(), args...));
  }

  // Callback that takes a net::Error as an int, for use with Mojo's UDPSocket.
  void OnWriteComplete(int32_t net_error);

  void UpdateIsWriteBlocked();
  void WritePacketHelper(const net::IPEndPoint endpoint,
                         base::span<const uint8_t> span);

  // Accessed on both the consumer's and the task runner's threads:
  std::size_t max_packets_in_flight_;
  std::atomic_bool is_write_blocked_;

  // TODO(jophba): When the media router moves to its own thread, remove
  // the additional thread hop to the task runner thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Only accessed on the task runner's thread:
  const std::unique_ptr<AsyncPacketSender> async_packet_sender_;
  Delegate* const delegate_;

  // Any time writable_ or packets_in_flight_ are changed, you should call
  // UpdateIsWriteBlocked().
  size_t packets_in_flight_ = 0;
  bool writable_ = true;

  base::WeakPtrFactory<NetworkServiceQuicPacketWriter> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NetworkServiceQuicPacketWriter);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_NETWORK_SERVICE_QUIC_PACKET_WRITER_H_
