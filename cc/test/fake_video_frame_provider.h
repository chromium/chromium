// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_
#define CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_

#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"

namespace cc {

// Fake video frame provider that always provides the same VideoFrame.
class FakeVideoFrameProvider : public VideoFrameProvider {
 public:
  FakeVideoFrameProvider();
  ~FakeVideoFrameProvider() override;

  void SetVideoFrameProviderClient(Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;
  base::TimeDelta GetPreferredRenderInterval() override;

  Client* client() { return client_; }

  void set_frame(scoped_refptr<media::VideoFrame> frame) {
    frame_ = std::move(frame);
  }

  int put_current_frame_count() const { return put_current_frame_count_; }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  Client* client_;
  int put_current_frame_count_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_
