// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_base.h"

namespace extensions {

class DesktopCaptureChooseDesktopMediaFunction
    : public DesktopCaptureChooseDesktopMediaFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("desktopCapture.chooseDesktopMedia",
                             DESKTOPCAPTURE_CHOOSEDESKTOPMEDIA)

  DesktopCaptureChooseDesktopMediaFunction();

 private:
  ~DesktopCaptureChooseDesktopMediaFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Returns the target name to show in the picker when capture is requested for
  // an extension.  Currently this is the same as the application name.
  std::string GetExtensionTargetName() const;
};

class DesktopCaptureCancelChooseDesktopMediaFunction
    : public DesktopCaptureCancelChooseDesktopMediaFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("desktopCapture.cancelChooseDesktopMedia",
                             DESKTOPCAPTURE_CANCELCHOOSEDESKTOPMEDIA)

  DesktopCaptureCancelChooseDesktopMediaFunction();

 private:
  ~DesktopCaptureCancelChooseDesktopMediaFunction() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_
