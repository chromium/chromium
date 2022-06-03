// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_DESKTOP_CAPTURE_PRIVATE_WEBRTC_DESKTOP_CAPTURE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_DESKTOP_CAPTURE_PRIVATE_WEBRTC_DESKTOP_CAPTURE_PRIVATE_API_H_

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_base.h"

namespace extensions {

class WebrtcDesktopCapturePrivateChooseDesktopMediaFunction
    : public DesktopCaptureChooseDesktopMediaFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcDesktopCapturePrivate.chooseDesktopMedia",
                             WEBRTCDESKTOPCAPTUREPRIVATE_CHOOSEDESKTOPMEDIA)
  WebrtcDesktopCapturePrivateChooseDesktopMediaFunction();

 private:
  ~WebrtcDesktopCapturePrivateChooseDesktopMediaFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction
    : public DesktopCaptureCancelChooseDesktopMediaFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "webrtcDesktopCapturePrivate.cancelChooseDesktopMedia",
      WEBRTCDESKTOPCAPTUREPRIVATE_CANCELCHOOSEDESKTOPMEDIA)

  WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction();

 private:
  ~WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_DESKTOP_CAPTURE_PRIVATE_WEBRTC_DESKTOP_CAPTURE_PRIVATE_API_H_
