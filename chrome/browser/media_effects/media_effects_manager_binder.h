// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_MANAGER_BINDER_H_
#define CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_MANAGER_BINDER_H_

#include "content/public/browser/browser_context.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"

namespace media_effects {

void BindVideoEffectsManager(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<media::mojom::VideoEffectsManager>
        video_effects_manager);

void BindVideoEffectsProcessor(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor);

}  // namespace media_effects

#endif  // CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_MANAGER_BINDER_H_
