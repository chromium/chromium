// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_PERMISSION_BUBBLE_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_PERMISSION_BUBBLE_MEDIA_ACCESS_HANDLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/media_access_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

// MediaAccessHandler for permission bubble requests.
class PermissionBubbleMediaAccessHandler
    : public MediaAccessHandler,
      public content::NotificationObserver {
 public:
  PermissionBubbleMediaAccessHandler();
  ~PermissionBubbleMediaAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(content::WebContents* web_contents,
                          const blink::mojom::MediaStreamType type,
                          const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const GURL& security_origin,
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

 private:
  struct PendingAccessRequest;
  using RequestsMap = std::map<int, PendingAccessRequest>;
  using RequestsMaps = std::map<content::WebContents*, RequestsMap>;

  void ProcessQueuedAccessRequest(content::WebContents* web_contents);
  void OnAccessRequestResponse(content::WebContents* web_contents,
                               int request_id,
                               const blink::MediaStreamDevices& devices,
                               blink::mojom::MediaStreamRequestResult result,
                               std::unique_ptr<content::MediaStreamUI> ui);

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  int next_request_id_ = 0;
  RequestsMaps pending_requests_;
  content::NotificationRegistrar notifications_registrar_;

  base::WeakPtrFactory<PermissionBubbleMediaAccessHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_PERMISSION_BUBBLE_MEDIA_ACCESS_HANDLER_H_
