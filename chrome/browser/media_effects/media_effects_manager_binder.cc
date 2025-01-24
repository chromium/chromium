// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_effects/media_effects_manager_binder.h"

#include "chrome/browser/media_effects/media_effects_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

namespace {

void BindReadonlyVideoEffectsManagerOnUIThread(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<media::mojom::ReadonlyVideoEffectsManager>
        readonly_video_effects_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* media_effects_service =
      MediaEffectsServiceFactory::GetForBrowserContext(browser_context);
  if (!media_effects_service) {
    LOG(WARNING) << "Video device not registered because no service was "
                    "returned for the current BrowserContext";
    return;
  }

  media_effects_service->BindReadonlyVideoEffectsManager(
      device_id, std::move(readonly_video_effects_manager));
}

void BindVideoEffectsProcessorOnUIThread(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* media_effects_service =
      MediaEffectsServiceFactory::GetForBrowserContext(browser_context);
  if (!media_effects_service) {
    LOG(WARNING) << "Video device not registered because no service was "
                    "returned for the current BrowserContext";
    return;
  }

  media_effects_service->BindVideoEffectsProcessor(
      device_id, std::move(video_effects_processor));
}
}  // namespace

namespace media_effects {

void BindVideoEffectsProcessor(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    BindVideoEffectsProcessorOnUIThread(device_id, browser_context,
                                        std::move(video_effects_processor));
    return;
  }
  // The function wasn't called from the UI thread, so post a task.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BindVideoEffectsProcessorOnUIThread, device_id,
                     browser_context, std::move(video_effects_processor)));
}

void BindReadonlyVideoEffectsManager(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<media::mojom::ReadonlyVideoEffectsManager>
        readonly_video_effects_manager) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    BindReadonlyVideoEffectsManagerOnUIThread(
        device_id, browser_context, std::move(readonly_video_effects_manager));
    return;
  }
  // The function wasn't called from the UI thread, so post a task.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&BindReadonlyVideoEffectsManagerOnUIThread,
                                device_id, browser_context,
                                std::move(readonly_video_effects_manager)));
}

base::WeakPtr<VideoEffectsManagerImpl> GetOrCreateVideoEffectsManager(
    const std::string& device_id,
    content::BrowserContext* browser_context) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser_context);

  auto* media_effects_service =
      MediaEffectsServiceFactory::GetForBrowserContext(browser_context);
  CHECK(media_effects_service);

  return media_effects_service->GetOrCreateVideoEffectsManager(device_id)
      .GetWeakPtr();
}

}  // namespace media_effects
