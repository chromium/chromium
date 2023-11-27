// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_H_

#include "third_party/webrtc/modules/desktop_capture/delegated_source_list_controller.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "ui/gfx/geometry/size.h"

// Class that is used to produces native desktop media thumbnails. Two modes are
// supported, delivering frames for a selected source on request (compatible
// with webrtc::DesktopCapturer) and delivering frames for selected sources on
// change.
class ThumbnailCapturer {
 public:
  // Reuse the existing types in webrtc::DesktopCapturer.
  using Result = webrtc::DesktopCapturer::Result;
  using SourceId = webrtc::DesktopCapturer::SourceId;
  using Source = webrtc::DesktopCapturer::Source;
  using SourceList = webrtc::DesktopCapturer::SourceList;

  enum class FrameDeliveryMethod {
    // Frames are delivered on request for the selected source when
    // CaptureFrame() is called.
    kOnRequest,
    // Frames are delivered recurrently at the specified maximum frame rate when
    // the selected sources are updated.
    kMultipleSourcesRecurrent,
  };

  class Consumer : public webrtc::DesktopCapturer::Callback {
   public:
    // Called recurrently after a new frame has been captured. `frame` is not
    // nullptr if and only if `result` is SUCCESS. `source_id` specifies the id
    // of the captured source.
    virtual void OnRecurrentCaptureResult(
        Result result,
        std::unique_ptr<webrtc::DesktopFrame> frame,
        SourceId source_id) = 0;

    // Called after the list of sources has been updated if the frame deliver
    // method is set to kMultipleSourcesRecurrent.
    virtual void OnSourceListUpdated() = 0;
  };

  virtual ~ThumbnailCapturer() = default;

  // Called at the beginning of a capturing session. If the frame
  // delivery method is kMultipleSourcesRecurrent, the capturer will begin
  // capturing the sources in the list that have been selected through the call
  // to SelectSources() and call Consumer::OnRecurrentCaptureResult() for each
  // captured frame. Consumer::OnSourceListUpdated() is called whenever the
  // source list is changed. `consumer` must remain valid until capturer is
  // destroyed.
  virtual void Start(Consumer* consumer) = 0;

  // Returns the frame delivery method that is used by the capturer.
  virtual FrameDeliveryMethod GetFrameDeliveryMethod() const = 0;

  // Sets max frame rate for the capturer. This is best effort and may not be
  // supported by all capturers. This will only affect the frequency at which
  // new frames are available, not the frequency at which you are allowed to
  // capture the frames.
  virtual void SetMaxFrameRate(uint32_t max_frame_rate);

  // Returns a valid pointer if the capturer requires the user to make a
  // selection from a source list provided by the capturer.
  // Returns nullptr if the capturer does not provide a UI for the user to make
  // a selection. The capturer must return the same value each time.
  //
  // Callers should not take ownership of the returned pointer, but it is
  // guaranteed to be valid as long as the desktop_capturer is valid.
  // Note that consumers should still use GetSourceList and SelectSource, but
  // their behavior may be modified if this returns a value. See those methods
  // for a more in-depth discussion of those potential modifications.
  virtual webrtc::DelegatedSourceListController*
  GetDelegatedSourceListController();

  // Captures next frame, and involve callback provided by Start() function.
  // Pending capture requests are canceled when DesktopCapturer is deleted. Can
  // only be invoked if the frame delivery method is kOnRequest.
  virtual void CaptureFrame();

  // Gets a list of sources current capturer supports. Returns false in case of
  // a failure.
  // For DesktopCapturer implementations to capture screens, this function
  // should return monitors.
  // For DesktopCapturer implementations to capture windows, this function
  // should only return root windows owned by applications.
  //
  // Note that capturers who use a delegated source list will return a
  // SourceList with exactly one value, but it may not be viable for capture
  // (e.g. CaptureFrame will return ERROR_TEMPORARY) until a selection has been
  // made.
  virtual bool GetSourceList(SourceList* sources) = 0;

  // Selects a source to be captured. Returns false in case of a failure (e.g.
  // if there is no source with the specified type and id.)
  //
  // Note that some capturers with delegated source lists may also support
  // selecting a SourceID that is not in the returned source list as a form of
  // restore token. Can only be invoked if the frame delivery method is
  // kOnRequest.
  virtual bool SelectSource(SourceId id);

  // Selects sources to be captured simultaneously. Multiple sources can only be
  // selected if the frame delivery method is kMultipleSourcesRecurrent.
  virtual void SelectSources(const std::vector<SourceId>& ids,
                             gfx::Size thumbnail_size);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_H_
