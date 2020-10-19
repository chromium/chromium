// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash/hash.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;
using content::DesktopMediaID;

namespace {

gfx::ImageSkia CreateEnclosedFaviconImage(gfx::Size size,
                                          const gfx::ImageSkia& favicon) {
  DCHECK_GE(size.width(), gfx::kFaviconSize);
  DCHECK_GE(size.height(), gfx::kFaviconSize);

  // Create a bitmap.
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height(), false);
  SkCanvas canvas(result);
  canvas.clear(SK_ColorTRANSPARENT);

  // Draw the favicon image into the center of result image. If the favicon is
  // too big, scale it down.
  gfx::Size fill_size = favicon.size();
  if (result.width() < favicon.width() || result.height() < favicon.height())
    fill_size = media::ScaleSizeToFitWithinTarget(favicon.size(), size);

  gfx::Rect center_rect(result.width(), result.height());
  center_rect.ClampToCenteredSize(fill_size);
  SkRect dest_rect =
      SkRect::MakeLTRB(center_rect.x(), center_rect.y(), center_rect.right(),
                       center_rect.bottom());
  canvas.drawBitmapRect(*favicon.bitmap(), dest_rect, nullptr);

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

// Update the list once per second.
const int kDefaultTabDesktopMediaListUpdatePeriod = 1000;

}  // namespace

TabDesktopMediaList::TabDesktopMediaList()
    : DesktopMediaListBase(base::TimeDelta::FromMilliseconds(
          kDefaultTabDesktopMediaListUpdatePeriod)) {
  type_ = DesktopMediaID::TYPE_WEB_CONTENTS;
  thumbnail_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

TabDesktopMediaList::~TabDesktopMediaList() {}

void TabDesktopMediaList::Refresh(bool update_thumnails) {
  DCHECK(can_refresh());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  if (!profile) {
    OnRefreshComplete();
    return;
  }

  std::vector<Browser*> browsers;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile()) {
      browsers.push_back(browser);
    }
  }

  ImageHashesMap new_favicon_hashes;
  std::vector<SourceDescription> sources;
  std::map<base::TimeTicks, SourceDescription> tab_map;
  std::vector<std::pair<DesktopMediaID, gfx::ImageSkia>> favicon_pairs;

  // Enumerate all tabs with their titles and favicons for a user profile.
  for (auto* browser : browsers) {
    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);

    for (int i = 0; i < tab_strip_model->count(); i++) {
      // Create id for tab.
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      DCHECK(contents);
      content::RenderFrameHost* main_frame = contents->GetMainFrame();
      DCHECK(main_frame);
      DesktopMediaID media_id(
          DesktopMediaID::TYPE_WEB_CONTENTS, DesktopMediaID::kNullId,
          content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                             main_frame->GetRoutingID()));

      // Get tab's last active time stamp.
      const base::TimeTicks t = contents->GetLastActiveTime();
      tab_map.insert(
          std::make_pair(t, SourceDescription(media_id, contents->GetTitle())));

      // Get favicon for tab.
      favicon::FaviconDriver* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(contents);
      if (!favicon_driver)
        continue;

      gfx::Image favicon = favicon_driver->GetFavicon();
      if (favicon.IsEmpty())
        continue;

      // Only new or changed favicon need update.
      new_favicon_hashes[media_id] = GetImageHash(favicon);
      if (!favicon_hashes_.count(media_id) ||
          (favicon_hashes_[media_id] != new_favicon_hashes[media_id])) {
        gfx::ImageSkia image = favicon.AsImageSkia();
        image.MakeThreadSafe();
        favicon_pairs.push_back(std::make_pair(media_id, image));
      }
    }
  }
  favicon_hashes_ = new_favicon_hashes;

  // Sort tab sources by time. Most recent one first. Then update sources list.
  for (auto it = tab_map.rbegin(); it != tab_map.rend(); ++it)
    sources.push_back(it->second);

  UpdateSourcesList(sources);

  for (const auto& it : favicon_pairs) {
    // Create a thumbail in a different thread and update the thumbnail in
    // current thread.
    base::PostTaskAndReplyWithResult(
        thumbnail_task_runner_.get(), FROM_HERE,
        base::BindOnce(&CreateEnclosedFaviconImage, thumbnail_size_, it.second),
        base::BindOnce(&TabDesktopMediaList::UpdateSourceThumbnail,
                       weak_factory_.GetWeakPtr(), it.first));
  }

  // OnRefreshComplete() needs to be called after all calls for
  // UpdateSourceThumbnail() have done. Therefore, a DoNothing task is posted to
  // the same sequenced task runner that CreateEnlargedFaviconImag() is posted.
  thumbnail_task_runner_.get()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&TabDesktopMediaList::OnRefreshComplete,
                     weak_factory_.GetWeakPtr()));
}
