// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_STORAGE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_STORAGE_FACTORY_H_

#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

void CreateMediaDrmStorage(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::MediaDrmStorage> receiver);

#endif  // CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_STORAGE_FACTORY_H_
