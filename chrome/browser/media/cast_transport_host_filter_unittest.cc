// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/callback.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/media/cast_transport_host_filter.h"
#include "chrome/common/cast_messages.h"
#include "content/public/test/browser_task_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CastTransportHostFilterTest : public testing::Test {
 public:
  CastTransportHostFilterTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    filter_ = new cast::CastTransportHostFilter();
    static_cast<cast::CastTransportHostFilter*>(filter_.get())
        ->InitializeNoOpWakeLockForTesting();
    // 127.0.0.1:7 is the local echo service port, which
    // is probably not going to respond, but that's ok.
    receive_endpoint_ = net::IPEndPoint(net::IPAddress::IPv4Localhost(), 7);
  }

 protected:
  void FakeSend(const IPC::Message& message) {
    EXPECT_TRUE(filter_->OnMessageReceived(message));
  }

  base::DictionaryValue options_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<content::BrowserMessageFilter> filter_;
  net::IPEndPoint receive_endpoint_;
};

TEST_F(CastTransportHostFilterTest, NewDelete) {
  const int kChannelId = 17;
  CastHostMsg_New new_msg(kChannelId,
                          receive_endpoint_,
                          net::IPEndPoint(),
                          options_);
  CastHostMsg_Delete delete_msg(kChannelId);

  // New, then delete, as expected.
  FakeSend(new_msg);
  FakeSend(delete_msg);
  FakeSend(new_msg);
  FakeSend(delete_msg);
  FakeSend(new_msg);
  FakeSend(delete_msg);

  // Now create/delete transport senders in the wrong order to make sure
  // this doesn't crash.
  FakeSend(new_msg);
  FakeSend(new_msg);
  FakeSend(new_msg);
  FakeSend(delete_msg);
  FakeSend(delete_msg);
  FakeSend(delete_msg);
}

TEST_F(CastTransportHostFilterTest, NewMany) {
  for (int i = 0; i < 100; i++) {
    CastHostMsg_New new_msg(i, receive_endpoint_, net::IPEndPoint(), options_);
    FakeSend(new_msg);
  }

  for (int i = 0; i < 60; i++) {
    CastHostMsg_Delete delete_msg(i);
    FakeSend(delete_msg);
  }

  // Leave some open, see what happens.
}

TEST_F(CastTransportHostFilterTest, SimpleMessages) {
  // Create a cast transport sender.
  const int32_t kChannelId = 42;
  CastHostMsg_New new_msg(kChannelId,
                          receive_endpoint_,
                          net::IPEndPoint(),
                          options_);
  FakeSend(new_msg);

  media::cast::CastTransportRtpConfig audio_config;
  audio_config.ssrc = 1;
  audio_config.feedback_ssrc = 2;
  audio_config.rtp_payload_type = media::cast::RtpPayloadType::AUDIO_OPUS;
  CastHostMsg_InitializeStream init_audio_msg(kChannelId, audio_config);
  FakeSend(init_audio_msg);

  media::cast::CastTransportRtpConfig video_config;
  video_config.ssrc = 11;
  video_config.feedback_ssrc = 12;
  video_config.rtp_payload_type = media::cast::RtpPayloadType::VIDEO_VP8;
  CastHostMsg_InitializeStream init_video_msg(kChannelId, video_config);
  FakeSend(init_video_msg);

  media::cast::EncodedFrame audio_frame;
  audio_frame.dependency = media::cast::EncodedFrame::KEY;
  audio_frame.frame_id = media::cast::FrameId::first() + 1;
  audio_frame.referenced_frame_id = media::cast::FrameId::first() + 1;
  const int kSamples = 47;
  audio_frame.rtp_timestamp = media::cast::RtpTimeTicks() +
      media::cast::RtpTimeDelta::FromTicks(kSamples);
  const int kBytesPerSample = 2;
  const int kChannels = 2;
  audio_frame.data = std::string(kSamples * kBytesPerSample * kChannels, 'q');
  CastHostMsg_InsertFrame insert_audio_frame(1, kChannelId, audio_frame);
  FakeSend(insert_audio_frame);

  media::cast::EncodedFrame video_frame;
  video_frame.dependency = media::cast::EncodedFrame::KEY;
  video_frame.frame_id = media::cast::FrameId::first() + 1;
  video_frame.referenced_frame_id = media::cast::FrameId::first() + 1;
  // Let's make sure we try a few kb so multiple packets
  // are generated.
  const int kVideoDataSize = 4711;
  video_frame.data = std::string(kVideoDataSize, 'p');
  CastHostMsg_InsertFrame insert_video_frame(11, kChannelId, video_frame);
  FakeSend(insert_video_frame);

  CastHostMsg_SendSenderReport rtcp_msg(
      kChannelId, 1, base::TimeTicks(),
      media::cast::RtpTimeTicks().Expand(UINT32_C(2)));
  FakeSend(rtcp_msg);

  std::vector<media::cast::FrameId> frame_ids;
  frame_ids.push_back(media::cast::FrameId::first() + 1);
  CastHostMsg_CancelSendingFrames cancel_msg(kChannelId, 1, frame_ids);
  FakeSend(cancel_msg);

  CastHostMsg_ResendFrameForKickstart kickstart_msg(
      kChannelId, 1, media::cast::FrameId::first() + 1);
  FakeSend(kickstart_msg);

  CastHostMsg_Delete delete_msg(kChannelId);
  FakeSend(delete_msg);
}

}  // namespace
