// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_PERMISSION_BUBBLE_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_PERMISSION_BUBBLE_MEDIA_ACCESS_HANDLER_H_

#include <stdint.h>

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "components/content_settings/core/common/content_settings.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// MediaAccessHandler for permission bubble requests.
class PermissionBubbleMediaAccessHandler
    : public MediaAccessHandler,
      public WebContentsCollection::Observer {
 public:
  PermissionBubbleMediaAccessHandler();
  ~PermissionBubbleMediaAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(content::WebContents* web_contents,
                          const blink::mojom::MediaStreamType type,
                          const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) override;
  void HandleRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     content::MediaResponseCallback callback,
                     const extensions::Extension* extension) override;
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               blink::mojom::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

  // Registers the prefs backing the audio and video policies.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  struct PendingAccessRequest;
  using RequestsMap = std::map<int64_t, PendingAccessRequest>;
  using RequestsMaps = std::map<content::WebContents*, RequestsMap>;

  void ProcessQueuedAccessRequest(
      MayBeDangling<content::WebContents> web_contents);
  void OnMediaStreamRequestResponse(
      content::WebContents* web_contents,
      int64_t request_id,
      content::MediaStreamRequest request,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      bool blocked_by_permissions_policy,
      ContentSetting audio_setting,
      ContentSetting video_setting);
  void OnAccessRequestResponse(
      content::WebContents* web_contents,
      int64_t request_id,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<content::MediaStreamUI> ui);
  // OnAccessRequestResponse cannot be used together with base::BindOnce as
  // StreamDevicesSet& cannot be captured (neither copyable nor movable).
  // This method uses StreamDevicesSetPtr (movable) and forwards the data
  // to OnAccessRequestResponse when calling the callback.
  void OnAccessRequestResponseForBinding(
      MayBeDangling<content::WebContents> web_contents,
      int64_t request_id,
      blink::mojom::StreamDevicesSetPtr stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<content::MediaStreamUI> ui);

  // WebContentsCollection::Observer:
  void WebContentsDestroyed(content::WebContents* web_contents) override;

  int64_t next_request_id_ = 0;
  RequestsMaps pending_requests_;

  WebContentsCollection web_contents_collection_;

  base::WeakPtrFactory<PermissionBubbleMediaAccessHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_PERMISSION_BUBBLE_MEDIA_ACCESS_HANDLER_H_
