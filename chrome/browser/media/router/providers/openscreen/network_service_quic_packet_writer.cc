// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/network_service_quic_packet_writer.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/third_party/quiche/src/quic/core/quic_constants.h"

namespace media_router {
namespace {

// Set a reasonable maximum number of packets in flight, for a total of
// 2 megabytes theoretical maximum. May need to change as device requirements
// become more clear here.
constexpr std::size_t kMaxPacketsInFlight =
    (2 * 1024 * 1024) / quic::kMaxOutgoingPacketSize;

const net::IPEndPoint ConvertToEndpoint(const quic::QuicSocketAddress& sa) {
  const std::string ip_bytes = sa.host().ToPackedString();
  CHECK(ip_bytes.length() == 4 || ip_bytes.length() == 16);
  const net::IPAddress ip_address(
      reinterpret_cast<const uint8_t*>(ip_bytes.data()), ip_bytes.length());
  return net::IPEndPoint(ip_address, sa.port());
}
}  // namespace

NetworkServiceQuicPacketWriter::NetworkServiceQuicPacketWriter(
    std::unique_ptr<AsyncPacketSender> async_packet_sender,
    NetworkServiceQuicPacketWriter::Delegate* delegate,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : max_packets_in_flight_(kMaxPacketsInFlight),
      is_write_blocked_(false),
      task_runner_(task_runner),
      async_packet_sender_(std::move(async_packet_sender)),
      delegate_(delegate) {
  DCHECK(async_packet_sender_ != nullptr);
  DCHECK(delegate_ != nullptr);
  DCHECK(task_runner_ != nullptr);
}

NetworkServiceQuicPacketWriter::~NetworkServiceQuicPacketWriter() = default;

quic::WriteResult NetworkServiceQuicPacketWriter::Flush() {
  // In PassThrough mode, this method isn't used.
  return quic::WriteResult(quic::WRITE_STATUS_OK, 0);
}

bool NetworkServiceQuicPacketWriter::IsBatchMode() const {
  // False here means we are always in PassThrough mode.
  return false;
}

bool NetworkServiceQuicPacketWriter::IsWriteBlocked() const {
  return is_write_blocked_;
}

quic::QuicByteCount NetworkServiceQuicPacketWriter::GetMaxPacketSize(
    const quic::QuicSocketAddress& peer_address) const {
  // In PassThrough mode, the max packet should be no larger than the max
  // quic packet size due to no batching.
  return quic::kMaxOutgoingPacketSize;
}

char* NetworkServiceQuicPacketWriter::GetNextWriteLocation(
    const quic::QuicIpAddress& self_address,
    const quic::QuicSocketAddress& peer_address) {
  // In PassThrough mode, this method isn't used and should return nullptr.
  return nullptr;
}

void NetworkServiceQuicPacketWriter::SetWritable() {
  if (!task_runner_->BelongsToCurrentThread()) {
    EnqueueTask(&NetworkServiceQuicPacketWriter::SetWritable);
    return;
  }

  writable_ = true;
  UpdateIsWriteBlocked();
}

bool NetworkServiceQuicPacketWriter::SupportsReleaseTime() const {
  return false;
}

quic::WriteResult NetworkServiceQuicPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const quic::QuicIpAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    quic::PerPacketOptions* options) {
  if (is_write_blocked_) {
    return quic::WriteResult(quic::WRITE_STATUS_BLOCKED, EWOULDBLOCK);
  }

  // The helper may run synchronously if we are on the right thread, else
  // it will enqueue a task.
  WritePacketHelper(
      ConvertToEndpoint(peer_address),
      base::make_span(reinterpret_cast<const uint8_t*>(buffer), buf_len));

  // Assume we successfully wrote the entire packet. The client will receive
  // any write errors through the delegate they are forced to provide us.
  return quic::WriteResult(quic::WRITE_STATUS_OK, buf_len);
}

void NetworkServiceQuicPacketWriter::OnWriteComplete(int net_error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    EnqueueTask(&NetworkServiceQuicPacketWriter::OnWriteComplete, net_error);
    return;
  }

  const net::Error error = static_cast<net::Error>(net_error);
  if (error != net::Error::OK) {
    delegate_->OnWriteError(error);
    writable_ = false;
  }

  --packets_in_flight_;
  UpdateIsWriteBlocked();
}

void NetworkServiceQuicPacketWriter::UpdateIsWriteBlocked() {
  const bool was_blocked = is_write_blocked_;

  is_write_blocked_ =
      !writable_ || (packets_in_flight_ >= max_packets_in_flight_);
  if (was_blocked && !is_write_blocked_) {
    delegate_->OnWriteUnblocked();
  }
}

void NetworkServiceQuicPacketWriter::WritePacketHelper(
    const net::IPEndPoint endpoint,
    base::span<const uint8_t> span) {
  if (!task_runner_->BelongsToCurrentThread()) {
    EnqueueTask(&NetworkServiceQuicPacketWriter::WritePacketHelper, endpoint,
                span);
    return;
  }

  const net::Error error = async_packet_sender_->SendTo(
      endpoint, span,
      base::BindOnce(&NetworkServiceQuicPacketWriter::OnWriteComplete,
                     weak_factory_.GetWeakPtr()));

  if (error != net::Error::OK) {
    delegate_->OnWriteError(error);
    writable_ = false;
    UpdateIsWriteBlocked();
    return;
  }

  ++packets_in_flight_;
  UpdateIsWriteBlocked();
}

}  // namespace media_router
