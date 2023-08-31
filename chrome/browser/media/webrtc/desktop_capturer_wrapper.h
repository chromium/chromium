// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURER_WRAPPER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURER_WRAPPER_H_

#include <memory>
#include <vector>

#include "chrome/browser/media/webrtc/thumbnail_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

// This class is a wrapper around a webrtc::DesktopCapturer object to make it
// possible to use it as a ThumbnailCapturer in NativeDesktopMediaList. All
// calls are forwarded directly to the webrtc::DesktopCapturer implementation.
class DesktopCapturerWrapper : public ThumbnailCapturer {
 public:
  explicit DesktopCapturerWrapper(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer);

  ~DesktopCapturerWrapper() override;

  void Start(Consumer* consumer) override;

  FrameDeliveryMethod GetFrameDeliveryMethod() const override;

  webrtc::DelegatedSourceListController* GetDelegatedSourceListController()
      override;

  void CaptureFrame() override;

  bool GetSourceList(SourceList* sources) override;

  bool SelectSource(SourceId id) override;

 private:
  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURER_WRAPPER_H_
