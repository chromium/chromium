// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_BASE_H_

#include <array>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "chrome/common/extensions/api/desktop_capture.h"
#include "url/gurl.h"

namespace extensions {

class DesktopCaptureChooseDesktopMediaFunctionBase
    : public ChromeAsyncExtensionFunction {
 public:
  // Used to set PickerFactory used to create mock DesktopMediaPicker instances
  // for tests. Calling tests keep ownership of the factory. Can be called with
  // |factory| set to NULL at the end of the test.
  static void SetPickerFactoryForTests(DesktopMediaPickerFactory* factory);

  DesktopCaptureChooseDesktopMediaFunctionBase();

  void Cancel();

 protected:
  ~DesktopCaptureChooseDesktopMediaFunctionBase() override;

  // |web_contents| is the WebContents for which the stream is created, and will
  // also be used to determine where to show the picker's UI.
  // |origin| is the origin for which the stream is created.
  // |target_name| is the display name of the stream target.
  bool Execute(const std::vector<
                   api::desktop_capture::DesktopCaptureSourceType>& sources,
               content::WebContents* web_contents,
               const GURL& origin,
               const base::string16 target_name);

  // Returns the calling application name to show in the picker.
  std::string GetCallerDisplayName() const;

  int request_id_;

 private:
  void OnPickerDialogResults(const GURL& origin,
                             content::WebContents* web_contents,
                             const std::string& err,
                             content::DesktopMediaID source);

  std::unique_ptr<DesktopMediaPickerController> picker_controller_;
};

class DesktopCaptureCancelChooseDesktopMediaFunctionBase
    : public ExtensionFunction {
 public:
  DesktopCaptureCancelChooseDesktopMediaFunctionBase();

 protected:
  ~DesktopCaptureCancelChooseDesktopMediaFunctionBase() override;

 private:
  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class DesktopCaptureRequestsRegistry {
 public:
  DesktopCaptureRequestsRegistry();
  ~DesktopCaptureRequestsRegistry();

  static DesktopCaptureRequestsRegistry* GetInstance();

  void AddRequest(int process_id,
                  int request_id,
                  DesktopCaptureChooseDesktopMediaFunctionBase* handler);
  void RemoveRequest(int process_id, int request_id);
  void CancelRequest(int process_id, int request_id);

 private:
  friend struct base::DefaultSingletonTraits<DesktopCaptureRequestsRegistry>;

  struct RequestId {
    RequestId(int process_id, int request_id);

    // Need to use RequestId as a key in std::map<>.
    bool operator<(const RequestId& other) const;

    int process_id;
    int request_id;
  };

  using RequestsMap =
      std::map<RequestId, DesktopCaptureChooseDesktopMediaFunctionBase*>;

  RequestsMap requests_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureRequestsRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_BASE_H_
