// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_transport_host_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/cast_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/udp_transport_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// The interval for CastTransport and/or CastRemotingSender to send
// Frame/PacketEvents to renderer process for logging.
constexpr base::TimeDelta kSendEventsInterval = base::TimeDelta::FromSeconds(1);

class TransportClient : public media::cast::CastTransport::Client {
 public:
  TransportClient(int32_t channel_id,
                  cast::CastTransportHostFilter* cast_transport_host_filter)
      : channel_id_(channel_id),
        cast_transport_host_filter_(cast_transport_host_filter) {}

  void OnStatusChanged(media::cast::CastTransportStatus status) final;
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
      std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events)
      final;
  void ProcessRtpPacket(std::unique_ptr<media::cast::Packet> packet) final;

 private:
  const int32_t channel_id_;
  cast::CastTransportHostFilter* const cast_transport_host_filter_;

  DISALLOW_COPY_AND_ASSIGN(TransportClient);
};

void TransportClient::OnStatusChanged(media::cast::CastTransportStatus status) {
  cast_transport_host_filter_->Send(
      new CastMsg_NotifyStatusChange(channel_id_, status));
}

void TransportClient::OnLoggingEventsReceived(
    std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
    std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events) {
  if (frame_events->empty() && packet_events->empty())
    return;
  cast_transport_host_filter_->Send(
      new CastMsg_RawEvents(channel_id_, *packet_events, *frame_events));
}

void TransportClient::ProcessRtpPacket(
    std::unique_ptr<media::cast::Packet> packet) {
  cast_transport_host_filter_->Send(
      new CastMsg_ReceivedPacket(channel_id_, *packet));
}

class RtcpClient : public media::cast::RtcpObserver {
 public:
  RtcpClient(
      int32_t channel_id,
      uint32_t rtp_sender_ssrc,
      base::WeakPtr<cast::CastTransportHostFilter> cast_transport_host_filter)
      : channel_id_(channel_id),
        rtp_sender_ssrc_(rtp_sender_ssrc),
        cast_transport_host_filter_(cast_transport_host_filter) {}

  void OnReceivedCastMessage(
      const media::cast::RtcpCastMessage& cast_message) override {
    if (cast_transport_host_filter_)
      cast_transport_host_filter_->Send(new CastMsg_RtcpCastMessage(
          channel_id_, rtp_sender_ssrc_, cast_message));
  }

  void OnReceivedRtt(base::TimeDelta round_trip_time) override {
    if (cast_transport_host_filter_)
      cast_transport_host_filter_->Send(
          new CastMsg_Rtt(channel_id_, rtp_sender_ssrc_, round_trip_time));
  }

  void OnReceivedPli() override {
    if (cast_transport_host_filter_)
      cast_transport_host_filter_->Send(
          new CastMsg_Pli(channel_id_, rtp_sender_ssrc_));
  }

 private:
  const int32_t channel_id_;
  const uint32_t rtp_sender_ssrc_;
  const base::WeakPtr<cast::CastTransportHostFilter>
      cast_transport_host_filter_;

  DISALLOW_COPY_AND_ASSIGN(RtcpClient);
};

void CastBindConnectorReceiver(
    mojo::PendingReceiver<service_manager::mojom::Connector>
        connector_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetSystemConnector()->BindConnectorReceiver(
      std::move(connector_receiver));
}

}  // namespace

namespace cast {

CastTransportHostFilter::CastTransportHostFilter()
    : BrowserMessageFilter(CastMsgStart) {}

CastTransportHostFilter::~CastTransportHostFilter() {}

void CastTransportHostFilter::OnStatusChanged(
    int32_t channel_id,
    media::cast::CastTransportStatus status) {
  Send(new CastMsg_NotifyStatusChange(channel_id, status));
}

bool CastTransportHostFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CastTransportHostFilter, message)
    IPC_MESSAGE_HANDLER(CastHostMsg_New, OnNew)
    IPC_MESSAGE_HANDLER(CastHostMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(CastHostMsg_InitializeStream, OnInitializeStream)
    IPC_MESSAGE_HANDLER(CastHostMsg_InsertFrame, OnInsertFrame)
    IPC_MESSAGE_HANDLER(CastHostMsg_SendSenderReport,
                        OnSendSenderReport)
    IPC_MESSAGE_HANDLER(CastHostMsg_ResendFrameForKickstart,
                        OnResendFrameForKickstart)
    IPC_MESSAGE_HANDLER(CastHostMsg_CancelSendingFrames,
                        OnCancelSendingFrames)
    IPC_MESSAGE_HANDLER(CastHostMsg_AddValidRtpReceiver, OnAddValidRtpReceiver)
    IPC_MESSAGE_HANDLER(CastHostMsg_InitializeRtpReceiverRtcpBuilder,
                        OnInitializeRtpReceiverRtcpBuilder)
    IPC_MESSAGE_HANDLER(CastHostMsg_AddCastFeedback, OnAddCastFeedback)
    IPC_MESSAGE_HANDLER(CastHostMsg_AddPli, OnAddPli)
    IPC_MESSAGE_HANDLER(CastHostMsg_AddRtcpEvents, OnAddRtcpEvents)
    IPC_MESSAGE_HANDLER(CastHostMsg_AddRtpReceiverReport,
                        OnAddRtpReceiverReport)
    IPC_MESSAGE_HANDLER(CastHostMsg_SendRtcpFromRtpReceiver,
                        OnSendRtcpFromRtpReceiver)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CastTransportHostFilter::OnNew(int32_t channel_id,
                                    const net::IPEndPoint& local_end_point,
                                    const net::IPEndPoint& remote_end_point,
                                    const base::DictionaryValue& options) {
  if (id_map_.IsEmpty()) {
    DVLOG(1) << ("Preventing the application from being suspended while one or "
                 "more transports are active for Cast Streaming.");
    GetWakeLock()->RequestWakeLock();
  }

  if (id_map_.Lookup(channel_id)) {
    id_map_.Remove(channel_id);
  }

  auto udp_transport = std::make_unique<media::cast::UdpTransportImpl>(
      base::ThreadTaskRunnerHandle::Get(), local_end_point, remote_end_point,
      base::BindRepeating(&CastTransportHostFilter::OnStatusChanged,
                          weak_factory_.GetWeakPtr(), channel_id));
  udp_transport->SetUdpOptions(options);
  std::unique_ptr<media::cast::CastTransport> transport =
      media::cast::CastTransport::Create(
          base::DefaultTickClock::GetInstance(), kSendEventsInterval,
          std::make_unique<TransportClient>(channel_id, this),
          std::move(udp_transport), base::ThreadTaskRunnerHandle::Get());
  transport->SetOptions(options);
  id_map_.AddWithID(std::move(transport), channel_id);
}

void CastTransportHostFilter::OnDelete(int32_t channel_id) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    id_map_.Remove(channel_id);
  } else {
    DVLOG(1) << "CastTransportHostFilter::Delete called "
             << "on non-existing channel";
  }

  // Delete all existing remoting senders for this channel.
  const auto entries = stream_id_map_.equal_range(channel_id);
  for (auto it = entries.first; it != entries.second; ++it) {
    if (remoting_sender_map_.Lookup(it->second)) {
      DVLOG(3) << "Delete CastRemotingSender for stream: " << it->second;
      remoting_sender_map_.Remove(it->second);
    }
  }
  stream_id_map_.erase(channel_id);

  if (id_map_.IsEmpty()) {
    DVLOG(1)
        << ("Releasing the block on application suspension since no transports "
            "are active anymore for Cast Streaming.");
    GetWakeLock()->CancelWakeLock();
  }
}

void CastTransportHostFilter::OnInitializeStream(
    int32_t channel_id,
    const media::cast::CastTransportRtpConfig& config) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    if (config.rtp_payload_type == media::cast::RtpPayloadType::REMOTE_AUDIO ||
        config.rtp_payload_type == media::cast::RtpPayloadType::REMOTE_VIDEO) {
      // Create CastRemotingSender for this RTP stream.
      remoting_sender_map_.AddWithID(
          std::make_unique<mirroring::CastRemotingSender>(
              transport, config, kSendEventsInterval,
              base::BindRepeating(
                  &CastTransportHostFilter::OnCastRemotingSenderEvents,
                  weak_factory_.GetWeakPtr(), channel_id)),
          config.rtp_stream_id);
      DVLOG(3) << "Create CastRemotingSender for stream: "
               << config.rtp_stream_id;

      stream_id_map_.insert(std::make_pair(channel_id, config.rtp_stream_id));
    } else {
      transport->InitializeStream(
          config, std::make_unique<RtcpClient>(channel_id, config.ssrc,
                                               weak_factory_.GetWeakPtr()));
    }
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnInitializeStream on non-existing "
                "channel";
  }
}

void CastTransportHostFilter::OnInsertFrame(
    int32_t channel_id,
    uint32_t ssrc,
    const media::cast::EncodedFrame& frame) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->InsertFrame(ssrc, frame);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInsertFrame on non-existing channel";
  }
}

void CastTransportHostFilter::OnCancelSendingFrames(
    int32_t channel_id,
    uint32_t ssrc,
    const std::vector<media::cast::FrameId>& frame_ids) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->CancelSendingFrames(ssrc, frame_ids);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnCancelSendingFrames "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnResendFrameForKickstart(
    int32_t channel_id,
    uint32_t ssrc,
    media::cast::FrameId frame_id) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->ResendFrameForKickstart(ssrc, frame_id);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnResendFrameForKickstart "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnSendSenderReport(
    int32_t channel_id,
    uint32_t ssrc,
    base::TimeTicks current_time,
    media::cast::RtpTimeTicks current_time_as_rtp_timestamp) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->SendSenderReport(ssrc, current_time,
                                current_time_as_rtp_timestamp);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnSendSenderReport "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnAddValidRtpReceiver(
    int32_t channel_id,
    uint32_t rtp_sender_ssrc,
    uint32_t rtp_receiver_ssrc) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->AddValidRtpReceiver(rtp_sender_ssrc, rtp_receiver_ssrc);
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnAddValidRtpReceiver "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnInitializeRtpReceiverRtcpBuilder(
    int32_t channel_id,
    uint32_t rtp_receiver_ssrc,
    const media::cast::RtcpTimeData& time_data) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->InitializeRtpReceiverRtcpBuilder(rtp_receiver_ssrc, time_data);
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnInitializeRtpReceiverRtcpBuilder "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnAddCastFeedback(
    int32_t channel_id,
    const media::cast::RtcpCastMessage& cast_message,
    base::TimeDelta target_delay) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->AddCastFeedback(cast_message, target_delay);
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnAddCastFeedback "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnAddPli(
    int32_t channel_id,
    const media::cast::RtcpPliMessage& pli_message) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->AddPli(pli_message);
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnAddPli on non-existing channel";
  }
}

void CastTransportHostFilter::OnAddRtcpEvents(
    int32_t channel_id,
    const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->AddRtcpEvents(rtcp_events);
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnAddRtcpEvents "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnAddRtpReceiverReport(
    int32_t channel_id,
    const media::cast::RtcpReportBlock& rtp_receiver_report_block) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->AddRtpReceiverReport(rtp_receiver_report_block);
  } else {
    DVLOG(1) << "CastTransportHostFilter::OnAddRtpReceiverReport "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnSendRtcpFromRtpReceiver(int32_t channel_id) {
  media::cast::CastTransport* transport = id_map_.Lookup(channel_id);
  if (transport) {
    transport->SendRtcpFromRtpReceiver();
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnSendRtcpFromRtpReceiver "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnCastRemotingSenderEvents(
    int32_t channel_id,
    const std::vector<media::cast::FrameEvent>& events) {
  if (events.empty())
    return;
  // PacketEvents can only come from CastTransport via CastTransport::Client
  // interface.
  Send(new CastMsg_RawEvents(channel_id,
                             std::vector<media::cast::PacketEvent>(), events));
}

device::mojom::WakeLock* CastTransportHostFilter::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (wake_lock_)
    return wake_lock_.get();

  mojo::PendingReceiver<service_manager::mojom::Connector> connector_receiver;
  auto connector = service_manager::Connector::Create(&connector_receiver);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&CastBindConnectorReceiver,
                                std::move(connector_receiver)));

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  connector->Connect(device::mojom::kServiceName,
                     wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther,
      "Cast is streaming content to a remote receiver",
      wake_lock_.BindNewPipeAndPassReceiver());
  return wake_lock_.get();
}

void CastTransportHostFilter::InitializeNoOpWakeLockForTesting() {
  // Initializes |wake_lock_| to make GetWakeLock() short-circuit out of its
  // own lazy initialization process.
  ignore_result(wake_lock_.BindNewPipeAndPassReceiver());
}

}  // namespace cast
