// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "chrome/browser/media/webrtc/select_audio_output_picker.h"
#include "components/webrtc/media_capture_dispatcher.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

class BrowserWindowInterface;
class DesktopCaptureAccessHandler;
class MediaAccessHandler;
class MediaStreamCaptureIndicator;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This singleton is used to receive updates about media events from the content
// layer.
class MediaCaptureDevicesDispatcher
    : public content::MediaObserver,
      public webrtc::MediaCaptureDispatcher,
      public webrtc::MediaStreamDeviceEnumeratorImpl {
 public:
  static MediaCaptureDevicesDispatcher* GetInstance();

  MediaCaptureDevicesDispatcher(const MediaCaptureDevicesDispatcher&) = delete;
  MediaCaptureDevicesDispatcher& operator=(
      const MediaCaptureDevicesDispatcher&) = delete;

  // Registers the preferences related to Media Stream default devices.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the security origin is associated with casting.
  static bool IsOriginForCasting(const GURL& origin);

  // Method called from WebCapturerDelegate implementations to process access
  // requests. |extension| is set to NULL if request was made from a drive-by
  // page.
  void ProcessMediaAccessRequest(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 const extensions::Extension* extension);

  // webrtc::MediaCaptureDispatcher:
  // Same as above with a null `extension`.
  void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

#if defined(TOOLKIT_VIEWS)
  void ProcessSelectAudioOutputRequest(
      BrowserWindowInterface* browser,
      const content::SelectAudioOutputRequest& request,
      content::SelectAudioOutputCallback callback);
#endif

  // Method called from WebCapturerDelegate implementations to check media
  // access permission. Note that this does not query the user.
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  // Same as above but for an |extension|, which may not be NULL.
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type,
                                  const extensions::Extension* extension);

  // Unittests that do not require actual device enumeration should call this
  // API on the singleton. It is safe to call this multiple times on the
  // signleton.
  void DisableDeviceEnumerationForTesting();

  // webrtc::MediaStreamDeviceEnumeratorImpl:
  const blink::MediaStreamDevices& GetAudioCaptureDevices() const override;
  const blink::MediaStreamDevices& GetVideoCaptureDevices() const override;
  const std::optional<blink::MediaStreamDevice>
  GetPreferredAudioDeviceForBrowserContext(
      content::BrowserContext* browser_context,
      const std::vector<std::string>& eligible_audio_device_ids) const override;
  const std::optional<blink::MediaStreamDevice>
  GetPreferredVideoDeviceForBrowserContext(
      content::BrowserContext* browser_context,
      const std::vector<std::string>& eligible_video_device_ids) const override;

  // content::MediaObserver:
  void OnAudioCaptureDevicesChanged() override;
  void OnVideoCaptureDevicesChanged() override;
  void OnMediaRequestStateChanged(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType stream_type,
                                  content::MediaRequestState state) override;
  void OnCreatingAudioStream(int render_process_id,
                             int render_frame_id) override;
  void OnSetCapturingLinkSecured(int render_process_id,
                                 int render_frame_id,
                                 int page_request_id,
                                 blink::mojom::MediaStreamType stream_type,
                                 bool is_secure) override;

  scoped_refptr<MediaStreamCaptureIndicator> GetMediaStreamCaptureIndicator();

  // Return true if there is any ongoing insecured capturing. The capturing is
  // deemed secure if all connected video sinks are reported secure and the
  // extension is trusted.
  bool IsInsecureCapturingInProgress(int render_process_id,
                                     int render_frame_id);

  // Only for testing.
  void SetTestAudioCaptureDevices(const blink::MediaStreamDevices& devices);
  void SetTestVideoCaptureDevices(const blink::MediaStreamDevices& devices);

  DesktopCaptureAccessHandler* desktop_capture_access_handler_for_test() {
    return desktop_capture_access_handler_for_test_;
  }

 private:
  friend class MediaCaptureDevicesDispatcherTest;

  friend struct base::DefaultSingletonTraits<MediaCaptureDevicesDispatcher>;

  MediaCaptureDevicesDispatcher();
  ~MediaCaptureDevicesDispatcher() override;

  // Called by the MediaObserver() functions, executed on UI thread.
  void NotifyAudioDevicesChangedOnUIThread();
  void NotifyVideoDevicesChangedOnUIThread();
  void UpdateMediaRequestStateOnUIThread(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      blink::mojom::MediaStreamType stream_type,
      content::MediaRequestState state);
  void OnCreatingAudioStreamOnUIThread(int render_process_id,
                                       int render_frame_id);
  void UpdateVideoScreenCaptureStatus(int render_process_id,
                                      int render_frame_id,
                                      int page_request_id,
                                      blink::mojom::MediaStreamType stream_type,
                                      bool is_secure);

  // Only for testing, a list of cached audio capture devices.
  blink::MediaStreamDevices test_audio_devices_;

  // Only for testing, a list of cached video capture devices.
  blink::MediaStreamDevices test_video_devices_;

  // Flag used by unittests to disable device enumeration.
  bool is_device_enumeration_disabled_;

  std::unique_ptr<SelectAudioOutputPicker> picker_views_;

  scoped_refptr<MediaStreamCaptureIndicator> media_stream_capture_indicator_;

  // Handlers for processing media access requests.
  std::vector<std::unique_ptr<MediaAccessHandler>> media_access_handlers_;

  // Only for testing, the desktop capture access handler, if one exists.
  raw_ptr<DesktopCaptureAccessHandler>
      desktop_capture_access_handler_for_test_ = nullptr;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
