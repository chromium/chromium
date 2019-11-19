// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_FRAME_PROVIDER_H_
#define CC_LAYERS_VIDEO_FRAME_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/cc_export.h"

namespace media {
class VideoFrame;
}

namespace cc {

// VideoFrameProvider and VideoFrameProvider::Client define the relationship by
// which video frames are exchanged between a provider and client.
//
// Threading notes: This class may be used in a multithreaded manner. However,
// if the Client implementation calls GetCurrentFrame()/PutCurrentFrame() from
// one thread, the provider must ensure that all client methods (except
// StopUsingProvider()) are called from that thread (typically the compositor
// thread).
class CC_EXPORT VideoFrameProvider {
 public:
  class CC_EXPORT Client {
   public:
    // The provider will call this method to tell the client to stop using it.
    // StopUsingProvider() may be called from any thread. The client should
    // block until it has PutCurrentFrame() any outstanding frames.
    virtual void StopUsingProvider() = 0;

    // Notifies the client that it should start or stop making regular
    // UpdateCurrentFrame() calls to the provider. No further calls to
    // UpdateCurrentFrame() should be made once StopRendering() returns.
    //
    // Callers should use these methods to indicate when it expects and no
    // longer expects (respectively) to have new frames for the client. Clients
    // may use this information for power conservation.
    //
    // Note that the client may also choose to stop driving frame updates, such
    // as if it believes that the frames are not visible.  In this case, the
    // client should report this via IsDrivingFrameUpdates().
    virtual void StartRendering() = 0;
    virtual void StopRendering() = 0;

    // Notifies the client that GetCurrentFrame() will return new data.
    virtual void DidReceiveFrame() = 0;

    // Should return true if and only if the client is actively driving frame
    // updates.  Note that this implies that the client has been told to
    // StartRendering by the VideoFrameProvider.  However, it's okay if the
    // client chooses to elide these calls, for example to save power when the
    // client knows that the frames are not visible.
    virtual bool IsDrivingFrameUpdates() const = 0;

   protected:
    virtual ~Client() {}
  };

  // May be called from any thread, but there must be some external guarantee
  // that the provider is not destroyed before this call returns.
  virtual void SetVideoFrameProviderClient(Client* client) = 0;

  // Called by the client on a regular interval. Returns true if a new frame
  // will be available via GetCurrentFrame() which should be displayed within
  // the presentation interval [|deadline_min|, |deadline_max|].
  //
  // Implementations may use this to drive frame acquisition from underlying
  // sources, so it must be called by clients before calling GetCurrentFrame().
  virtual bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                                  base::TimeTicks deadline_max) = 0;

  // Returns true if GetCurrentFrame() will return a non-null frame and false
  // otherwise. Aside from thread locks, the state won't change.
  virtual bool HasCurrentFrame() = 0;

  // Returns the current frame, which may have been updated by a recent call to
  // UpdateCurrentFrame(). A call to this method does not ensure that the frame
  // will be rendered. A subsequent call to PutCurrentFrame() must be made if
  // the frame is expected to be rendered.
  //
  // Clients should call this in response to UpdateCurrentFrame() returning true
  // or in response to a DidReceiveFrame() call.
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() = 0;

  // Called in response to DidReceiveFrame() or a return value of true from
  // UpdateCurrentFrame() if the current frame was considered for rendering; the
  // frame may not been rendered for a variety of reasons (occlusion, etc).
  // Providers may use the absence of this call as a signal to detect when a new
  // frame missed its intended deadline.
  virtual void PutCurrentFrame() = 0;

  // Returns the interval at which the provider expects to have new frames for
  // the client.
  virtual base::TimeDelta GetPreferredRenderInterval() = 0;

 protected:
  virtual ~VideoFrameProvider() {}
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_FRAME_PROVIDER_H_
