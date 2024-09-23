// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/current_tab_desktop_media_list.h"

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace {

constexpr base::TimeDelta kUpdatePeriodMs = base::Milliseconds(1000);

void HandleCapturedBitmap(
    base::OnceCallback<void(uint32_t, const std::optional<gfx::ImageSkia>&)>
        reply,
    std::optional<uint32_t> last_hash,
    gfx::Size thumbnail_size,
    const SkBitmap& bitmap) {
  DCHECK(!thumbnail_size.IsEmpty());

  std::optional<gfx::ImageSkia> image;

  // Only scale and update if the frame appears to be new.
  const uint32_t hash = base::FastHash(base::make_span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize()));
  if (!last_hash.has_value() || hash != last_hash.value()) {
    image = ScaleBitmap(bitmap, thumbnail_size);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(reply), hash, image));
}

}  // namespace

CurrentTabDesktopMediaList ::CurrentTabDesktopMediaList(
    content::WebContents* web_contents)
    : CurrentTabDesktopMediaList(web_contents, kUpdatePeriodMs, nullptr) {}

CurrentTabDesktopMediaList::CurrentTabDesktopMediaList(
    content::WebContents* web_contents,
    base::TimeDelta period,
    DesktopMediaListObserver* observer)
    : DesktopMediaListBase(period),
      media_id_(content::DesktopMediaID::TYPE_WEB_CONTENTS,
                content::DesktopMediaID::kNullId,
                content::WebContentsMediaCaptureId(
                    web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
                    web_contents->GetPrimaryMainFrame()->GetRoutingID())),
      thumbnail_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  DCHECK(web_contents);

  type_ = DesktopMediaList::Type::kCurrentTab;

  if (observer) {
    StartUpdating(observer);
  }

  // The source never changes - it always applies to the current tab.
  UpdateSourcesList({SourceDescription(media_id_, std::u16string())});
}

CurrentTabDesktopMediaList::~CurrentTabDesktopMediaList() = default;

void CurrentTabDesktopMediaList::Refresh(bool update_thumbnails) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(can_refresh());

  if (refresh_in_progress_ || !update_thumbnails || thumbnail_size_.IsEmpty()) {
    return;
  }

  content::RenderFrameHost* const host = content::RenderFrameHost::FromID(
      media_id_.web_contents_id.render_process_id,
      media_id_.web_contents_id.main_render_frame_id);
  if (!host) {
    return;
  }

  content::RenderWidgetHostView* const view = host->GetView();
  if (!view) {
    return;
  }

  refresh_in_progress_ = true;

  auto reply = base::BindOnce(&CurrentTabDesktopMediaList::OnCaptureHandled,
                              weak_factory_.GetWeakPtr());

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindPostTask(thumbnail_task_runner_,
                         base::BindOnce(&HandleCapturedBitmap, std::move(reply),
                                        last_hash_, thumbnail_size_)));
}

void CurrentTabDesktopMediaList::OnCaptureHandled(
    uint32_t hash,
    const std::optional<gfx::ImageSkia>& image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK((hash != last_hash_) == image.has_value());  // Only new frames passed.

  refresh_in_progress_ = false;

  if (hash != last_hash_) {
    last_hash_ = hash;
    UpdateSourceThumbnail(media_id_, image.value());
  }

  OnRefreshComplete();
}

void CurrentTabDesktopMediaList::ResetLastHashForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_hash_.reset();
}
