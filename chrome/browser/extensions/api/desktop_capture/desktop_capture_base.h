// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_BASE_H_

#include <array>
#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "chrome/common/extensions/api/desktop_capture.h"
#include "extensions/browser/extension_function.h"
#include "url/gurl.h"

namespace extensions {

class DesktopCaptureChooseDesktopMediaFunctionBase : public ExtensionFunction {
 public:
  // Used to set PickerFactory used to create mock DesktopMediaPicker instances
  // for tests. Calling tests keep ownership of the factory. Can be called with
  // |factory| set to NULL at the end of the test.
  static void SetPickerFactoryForTests(DesktopMediaPickerFactory* factory);

  DesktopCaptureChooseDesktopMediaFunctionBase();

  void Cancel();

 protected:
  ~DesktopCaptureChooseDesktopMediaFunctionBase() override;

  static const char kTargetNotFoundError[];

  // |exclude_system_audio| and |exclude_self_browser_surface| are piped from
  // the original call. They are constraints that need to be applied before
  // the picker is shown to the user, as they affect the picker.
  // It is therefore provided to the extension function rather than
  // to getUserMedia(), as is the case for other constraints.
  //
  // |suppress_local_audio_playback_intended| is an indication of what the
  // application *intends* to set. This flag is necessary because the
  // media-picker shown to the user shows a warning that local system audio will
  // be suppressed when that is the case, so this flag has to be plumbed down
  // with the other flags that affect the picker. However, unlike the other
  // flags, this one only communicates intent, and has no other effect.
  // It is a necessary evil, a work-around to address crbug.com/1354189.
  // This will go away once the Extension API itself goes away.
  //
  // |origin| is the origin for which the stream is created.
  //
  // |target_name| is the display name of the stream target.
  ResponseAction Execute(
      const std::vector<api::desktop_capture::DesktopCaptureSourceType>&
          sources,
      bool exclude_system_audio,
      bool exclude_self_browser_surface,
      bool suppress_local_audio_playback_intended,
      content::RenderFrameHost* render_frame_host,
      const GURL& origin,
      const std::u16string& target_name);

  // Returns the calling application name to show in the picker.
  std::string GetCallerDisplayName() const;

  int request_id_;

 private:
  void OnPickerDialogResults(
      const GURL& origin,
      const content::GlobalRenderFrameHostId& render_frame_host_id,
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

  DesktopCaptureRequestsRegistry(const DesktopCaptureRequestsRegistry&) =
      delete;
  DesktopCaptureRequestsRegistry& operator=(
      const DesktopCaptureRequestsRegistry&) = delete;

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

  using RequestsMap = std::map<
      RequestId,
      raw_ptr<DesktopCaptureChooseDesktopMediaFunctionBase, CtnExperimental>>;

  RequestsMap requests_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_BASE_H_
