// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"

#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/media/webrtc/desktop_media_list_layout_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/favicon_size.h"

using content::BrowserThread;
using content::DesktopMediaID;
using content::WebContents;

namespace {

gfx::ImageSkia CreateEnclosedFaviconImage(gfx::Size size,
                                          const gfx::ImageSkia& favicon) {
  DCHECK_GE(size.width(), gfx::kFaviconSize);
  DCHECK_GE(size.height(), gfx::kFaviconSize);

  // Create a bitmap.
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height(), false);
  SkCanvas canvas(result, SkSurfaceProps{});
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
  canvas.drawImageRect(favicon.bitmap()->asImage(), dest_rect,
                       SkSamplingOptions());

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

// Update the list once per second.
const int kDefaultTabDesktopMediaListUpdatePeriod = 1000;

void HandleCapturedBitmap(
    base::OnceCallback<void(uint32_t, const gfx::ImageSkia&)> reply,
    std::optional<uint32_t> last_hash,
    const SkBitmap& bitmap) {
  gfx::ImageSkia image;

  // Only scale and update if the frame appears to be new.
  const uint32_t hash = base::FastHash(base::make_span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize()));
  if (!last_hash.has_value() || hash != last_hash.value()) {
    image = ScaleBitmap(bitmap, desktopcapture::kPreviewSize);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(reply), hash, image));
}

}  // namespace

TabDesktopMediaList::TabDesktopMediaList(
    WebContents* web_contents,
    DesktopMediaList::WebContentsFilter includable_web_contents_filter,
    bool include_chrome_app_windows)
    : DesktopMediaListBase(
          base::Milliseconds(kDefaultTabDesktopMediaListUpdatePeriod)),
      web_contents_(web_contents
                        ? std::make_optional(web_contents->GetWeakPtr())
                        : std::nullopt),
      includable_web_contents_filter_(
          std::move(includable_web_contents_filter)),
      include_chrome_app_windows_(include_chrome_app_windows) {
  type_ = DesktopMediaList::Type::kWebContents;
  image_resize_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

TabDesktopMediaList::~TabDesktopMediaList() {
  // previewed_source_visible_keepalive_ is expected to be destructed on the UI
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void TabDesktopMediaList::CompleteRefreshAfterThumbnailProcessing() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // OnRefreshComplete() needs to be called after all calls to
  // UpdateSourceThumbnail() have completed. Therefore, a DoNothing task is
  // posted to the same sequenced task runner to which
  // CreateEnclosedFaviconImage() is posted.
  image_resize_task_runner_.get()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&TabDesktopMediaList::OnRefreshComplete,
                     weak_factory_.GetWeakPtr()));
}

void TabDesktopMediaList::Refresh(bool update_thumnails) {
  DCHECK(can_refresh());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile;
  if (web_contents_.has_value()) {
    const base::WeakPtr<WebContents>& wc_weak_ref = web_contents_.value();
    // Profile::FromBrowserContext is robust to receiving nullptr as input.
    profile = Profile::FromBrowserContext(
        wc_weak_ref ? wc_weak_ref->GetBrowserContext() : nullptr);
  } else {
    // When going through DesktopMediaPickerController::Show(), it can be that
    // no WebContents was ever associated. In that case, fall back on the
    // legacy behavior of using the last-used profile.
    profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  }
  if (!profile) {
    OnRefreshComplete();
    return;
  }

  std::vector<Browser*> browsers;
  for (Browser* browser : *BrowserList::GetInstance()) {
    // Omit all the IWAs for TabDesktopMediaList as they are already
    // present in NativeDesktopMediaList.
    bool is_isolated_web_app = browser->app_controller() &&
                               browser->app_controller()->IsIsolatedWebApp();

    if ((!base::FeatureList::IsEnabled(
             features::kRemovalOfIWAsFromTabCapture) ||
         !is_isolated_web_app) &&
        browser->profile()->GetOriginalProfile() ==
            profile->GetOriginalProfile()) {
      browsers.push_back(browser);
    }
  }

  std::vector<WebContents*> contents_list;
  // Enumerate all tabs for a user profile.
  for (auto* browser : browsers) {
    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);

    for (int i = 0; i < tab_strip_model->count(); i++) {
      // Create id for tab.
      WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      DCHECK(contents);
      contents_list.push_back(contents);
    }
  }

  if (include_chrome_app_windows_) {
    // Find all AppWindows for the given profile.
    const extensions::AppWindowRegistry::AppWindowList& window_list =
        extensions::AppWindowRegistry::Get(profile)->app_windows();
    for (const extensions::AppWindow* app_window : window_list) {
      if (!app_window->is_hidden())
        contents_list.push_back(app_window->web_contents());
    }
  }

  ImageHashesMap new_favicon_hashes;
  std::vector<SourceDescription> sources;
  std::map<base::TimeTicks, SourceDescription> tab_map;
  std::vector<std::pair<DesktopMediaID, gfx::ImageSkia>> favicon_pairs;
  // Fetch title, favicons, and update time for all tabs to show.
  for (auto* contents : contents_list) {
    if (!includable_web_contents_filter_.Run(contents))
      continue;
    content::RenderFrameHost* main_frame = contents->GetPrimaryMainFrame();
    DCHECK(main_frame);
    DesktopMediaID media_id(
        DesktopMediaID::TYPE_WEB_CONTENTS, DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                           main_frame->GetRoutingID()));

    // Get tab's last active time stamp.
    const base::TimeTicks t = contents->GetLastActiveTimeTicks();
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
  favicon_hashes_ = new_favicon_hashes;

  // Sort tab sources by time. Most recent one first. Then update sources list.
  for (const auto& [time, tab_source] : base::Reversed(tab_map))
    sources.push_back(tab_source);

  UpdateSourcesList(sources);

  for (const auto& it : favicon_pairs) {
    // Create a thumbail in a different thread and update the thumbnail in
    // current thread.
    image_resize_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CreateEnclosedFaviconImage, thumbnail_size_, it.second),
        base::BindOnce(&TabDesktopMediaList::UpdateSourceThumbnail,
                       weak_factory_.GetWeakPtr(), it.first));
  }

  if (previewed_source_) {
    // Trigger an update of the selected tab's preview image. It handles calling
    // OnRefreshComplete when it's ready.
    TriggerScreenshot(/*remaining_retries=*/0,
                      std::make_unique<TabDesktopMediaList::RefreshCompleter>(
                          weak_factory_.GetWeakPtr()));
  } else {
    // No preview to update.
    CompleteRefreshAfterThumbnailProcessing();
  }
}

void TabDesktopMediaList::TriggerScreenshot(
    int remaining_retries,
    std::unique_ptr<TabDesktopMediaList::RefreshCompleter> refresh_completer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!previewed_source_.has_value()) {
    // The selection must have been cleared while waiting to retry. Nothing to
    // do.
    return;
  }

  content::RenderFrameHost* host = content::RenderFrameHost::FromID(
      previewed_source_->web_contents_id.render_process_id,
      previewed_source_->web_contents_id.main_render_frame_id);
  content::RenderWidgetHostView* const view = host ? host->GetView() : nullptr;
  if (!view) {
    // Clear the preview image, so that we don't have a stale image for eg
    // crashed tabs.
    UpdateSourcePreview(previewed_source_.value(), gfx::ImageSkia());
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindPostTask(
          content::GetUIThreadTaskRunner({}),
          base::BindOnce(&TabDesktopMediaList::ScreenshotReceived,
                         weak_factory_.GetWeakPtr(), remaining_retries,
                         previewed_source_.value(),
                         std::move(refresh_completer))));
}

void TabDesktopMediaList::ScreenshotReceived(
    int remaining_retries,
    const content::DesktopMediaID& id,
    std::unique_ptr<TabDesktopMediaList::RefreshCompleter> refresh_completer,
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (id != previewed_source_) {
    // Selection has changed since triggering this screenshot. Quit early to
    // avoid rescaling the image unnecessarily.
    return;
  }

  // TODO(crbug.com/40187992): Listen for a newly drawn frame to be ready when a
  // hidden tab is woken up,rather than just retrying after an arbitrary delay.
  constexpr base::TimeDelta kScreenshotRetryDelayMs = base::Milliseconds(20);

  // It can take a little time after we tell a WebContents it's being captured
  // by calling IncrementCapturerCount before it starts painting actual frames,
  // so do a few retries before giving up and proceeding with an empty image
  // meaning the preview is cleared.
  if (bitmap.drawsNothing() && remaining_retries > 0) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TabDesktopMediaList::TriggerScreenshot,
                       weak_factory_.GetWeakPtr(), remaining_retries - 1,
                       std::move(refresh_completer)),
        kScreenshotRetryDelayMs);
    return;
  }

  auto reply = base::BindOnce(&TabDesktopMediaList::OnPreviewCaptureHandled,
                              weak_factory_.GetWeakPtr(), id,
                              std::move(refresh_completer));
  image_resize_task_runner_.get()->PostTask(
      FROM_HERE, base::BindOnce(&HandleCapturedBitmap, std::move(reply),
                                last_hash_, bitmap));
}

void TabDesktopMediaList::OnPreviewCaptureHandled(
    const content::DesktopMediaID& media_id,
    std::unique_ptr<TabDesktopMediaList::RefreshCompleter> refresh_completer,
    uint32_t new_hash,
    const gfx::ImageSkia& image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (new_hash != last_hash_) {
    last_hash_ = new_hash;
    UpdateSourcePreview(media_id, image);
  }
}

void TabDesktopMediaList::SetPreviewedSource(
    const std::optional<content::DesktopMediaID>& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!(id.has_value() && id.value().is_null()));

  previewed_source_ = id;
  previewed_source_visible_keepalive_.RunAndReset();

  if (!id.has_value()) {
    return;
  }

  content::RenderFrameHost* const host = content::RenderFrameHost::FromID(
      id->web_contents_id.render_process_id,
      id->web_contents_id.main_render_frame_id);
  // Note host may be nullptr, but FromRenderFrameHost handles that for us.
  WebContents* const source_contents = WebContents::FromRenderFrameHost(host);
  if (!source_contents) {
    // No WebContents instance found, likely the selected tab has been recently
    // closed or crashed and the list of sources hasn't been updated yet.
    UpdateSourcePreview(id.value(), gfx::ImageSkia());
    return;
  }

  // Let the WebContents know that it's being visibly captured, so paints even
  // in the background. Pass false to stay_hidden to fully wake the page to not
  // only allow it to load, but also to avoid pages realising they're visible
  // only in the preview and manipulating the user.
  previewed_source_visible_keepalive_ = source_contents->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/false,
      /*is_activity=*/false);

  // Capture a new previewed image.
  // TODO(crbug.com/40187992): Schedule this delayed if there has been another
  // update recently to avoid churning when a user scrolls quickly through the
  // list.
  constexpr int kMaxPreviewRetries = 5;
  TriggerScreenshot(kMaxPreviewRetries, /*refresh_completer=*/nullptr);
}

TabDesktopMediaList::RefreshCompleter::RefreshCompleter(
    base::WeakPtr<TabDesktopMediaList> list)
    : list_(list) {}

TabDesktopMediaList::RefreshCompleter::~RefreshCompleter() {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI) && list_) {
    list_->CompleteRefreshAfterThumbnailProcessing();
  }
}
