// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class EnterpriseWebrtcStartCaptureFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.webrtc.startCapture",
                             ENTERPRISE_WEBRTC_STARTCAPTURE)

 protected:
  ~EnterpriseWebrtcStartCaptureFunction() override = default;
  ResponseAction Run() override;
};

class EnterpriseWebrtcGetCaptureStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.webrtc.getCaptureStatus",
                             ENTERPRISE_WEBRTC_GETCAPTURESTATUS)

 protected:
  ~EnterpriseWebrtcGetCaptureStatusFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_H_
