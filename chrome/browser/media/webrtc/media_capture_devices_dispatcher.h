// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

class MediaAccessHandler;
class MediaStreamCaptureIndicator;

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
      public webrtc::MediaStreamDeviceEnumeratorImpl {
 public:
  class Observer {
   public:
    // Handle an information update consisting of a up-to-date audio capture
    // device lists. This happens when a microphone is plugged in or unplugged.
    virtual void OnUpdateAudioDevices(
        const blink::MediaStreamDevices& devices) {}

    // Handle an information update consisting of a up-to-date video capture
    // device lists. This happens when a camera is plugged in or unplugged.
    virtual void OnUpdateVideoDevices(
        const blink::MediaStreamDevices& devices) {}

    // Handle an information update related to a media stream request.
    virtual void OnRequestUpdate(int render_process_id,
                                 int render_frame_id,
                                 blink::mojom::MediaStreamType stream_type,
                                 const content::MediaRequestState state) {}

    // Handle an information update that a new stream is being created.
    virtual void OnCreatingAudioStream(int render_process_id,
                                       int render_frame_id) {}

    virtual ~Observer() {}
  };

  static MediaCaptureDevicesDispatcher* GetInstance();

  MediaCaptureDevicesDispatcher(const MediaCaptureDevicesDispatcher&) = delete;
  MediaCaptureDevicesDispatcher& operator=(
      const MediaCaptureDevicesDispatcher&) = delete;

  // Registers the preferences related to Media Stream default devices.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the security origin is associated with casting.
  static bool IsOriginForCasting(const GURL& origin);

  // Methods for observers. Called on UI thread.
  // Observers should add themselves on construction and remove themselves
  // on destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Method called from WebCapturerDelegate implementations to process access
  // requests. |extension| is set to NULL if request was made from a drive-by
  // page.
  void ProcessMediaAccessRequest(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 const extensions::Extension* extension);

  // Method called from WebCapturerDelegate implementations to check media
  // access permission. Note that this does not query the user.
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type);

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

  // A list of observers for the device update notifications.
  base::ObserverList<Observer>::Unchecked observers_;

  // Flag used by unittests to disable device enumeration.
  bool is_device_enumeration_disabled_;

  scoped_refptr<MediaStreamCaptureIndicator> media_stream_capture_indicator_;

  // Handlers for processing media access requests.
  std::vector<std::unique_ptr<MediaAccessHandler>> media_access_handlers_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
