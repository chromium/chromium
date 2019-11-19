// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
#define CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "components/mirroring/browser/cast_remoting_sender.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace cast {

class CastTransportHostFilter : public content::BrowserMessageFilter {
 public:
  CastTransportHostFilter();

  // Used by unit test only.
  void InitializeNoOpWakeLockForTesting();

 private:
  ~CastTransportHostFilter() override;

  // Status callback to create UdpTransport.
  void OnStatusChanged(int32_t channel_id,
                       media::cast::CastTransportStatus status);

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // Forwarding functions.
  // For remoting RTP streams, calling this will create a CastRemotingSender for
  // the stream, which will be automatically destroyed when the associated
  // chanel is deleted.
  void OnInitializeStream(int32_t channel_id,
                          const media::cast::CastTransportRtpConfig& config);

  void OnInsertFrame(int32_t channel_id,
                     uint32_t ssrc,
                     const media::cast::EncodedFrame& frame);
  void OnSendSenderReport(
      int32_t channel_id,
      uint32_t ssrc,
      base::TimeTicks current_time,
      media::cast::RtpTimeTicks current_time_as_rtp_timestamp);
  void OnCancelSendingFrames(
      int32_t channel_id,
      uint32_t ssrc,
      const std::vector<media::cast::FrameId>& frame_ids);
  void OnResendFrameForKickstart(int32_t channel_id,
                                 uint32_t ssrc,
                                 media::cast::FrameId frame_id);
  void OnAddValidRtpReceiver(int32_t channel_id,
                             uint32_t rtp_sender_ssrc,
                             uint32_t rtp_receiver_ssrc);
  void OnInitializeRtpReceiverRtcpBuilder(
      int32_t channel_id,
      uint32_t rtp_receiver_ssrc,
      const media::cast::RtcpTimeData& time_data);
  void OnAddCastFeedback(int32_t channel_id,
                         const media::cast::RtcpCastMessage& cast_message,
                         base::TimeDelta target_delay);
  void OnAddPli(int32_t channel_id,
                const media::cast::RtcpPliMessage& pli_message);
  void OnAddRtcpEvents(
      int32_t channel_id,
      const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events);
  void OnAddRtpReceiverReport(
      int32_t channel_id,
      const media::cast::RtcpReportBlock& rtp_receiver_report_block);
  void OnSendRtcpFromRtpReceiver(int32_t channel_id);

  void OnNew(int32_t channel_id,
             const net::IPEndPoint& local_end_point,
             const net::IPEndPoint& remote_end_point,
             const base::DictionaryValue& options);
  void OnDelete(int32_t channel_id);

  // Sends frame events from CastRemotingSender to renderer process for logging.
  void OnCastRemotingSenderEvents(
      int32_t channel_id,
      const std::vector<media::cast::FrameEvent>& events);

  device::mojom::WakeLock* GetWakeLock();

  base::IDMap<std::unique_ptr<media::cast::CastTransport>> id_map_;

  // While |id_map_| is non-empty, we use |wake_lock_| to request and
  // hold a wake lock. This prevents Chrome from being suspended while remoting
  // content. If any wake lock is held upon destruction, it's implicitly
  // canceled when this object is destroyed.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  // This map records all active remoting senders. It uses the unique RTP
  // stream ID as the key.
  base::IDMap<std::unique_ptr<mirroring::CastRemotingSender>>
      remoting_sender_map_;

  // This map stores all active remoting streams for each channel. It uses the
  // channel ID as the key.
  std::multimap<int32_t, int32_t> stream_id_map_;

  base::WeakPtrFactory<CastTransportHostFilter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastTransportHostFilter);
};

}  // namespace cast

#endif  // CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
