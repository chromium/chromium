// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_H_

#include <map>

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/desktop_media_list_base.h"

// Implementation of DesktopMediaList that shows tab/WebContents.
class TabDesktopMediaList : public DesktopMediaListBase {
 public:
  TabDesktopMediaList(
      content::WebContents* web_contents,
      DesktopMediaList::WebContentsFilter includable_web_contents_filter,
      bool include_chrome_app_windows);

  TabDesktopMediaList(const TabDesktopMediaList&) = delete;
  TabDesktopMediaList& operator=(const TabDesktopMediaList&) = delete;

  ~TabDesktopMediaList() override;

  void SetPreviewedSource(
      const std::optional<content::DesktopMediaID>& id) override;

 private:
  class RefreshCompleter {
   public:
    explicit RefreshCompleter(const base::WeakPtr<TabDesktopMediaList> list);
    ~RefreshCompleter();

   private:
    base::WeakPtr<TabDesktopMediaList> list_;
  };

  typedef std::map<content::DesktopMediaID, uint32_t> ImageHashesMap;

  void Refresh(bool update_thumnails) override;

  // TODO(crbug.com/40187992): Combine the below logic for screenshotting with
  // the very similar behaviour in current_tab_desktop_media_list.h

  // Called on the UI thread after the captured image is handled. If the
  // image was new, it's rescaled to the desired size and sent back in |image|.
  // Otherwise, an empty Optional is sent back. In either case, |new_hash| is
  // the hash value of the frame that was handled.
  void OnPreviewCaptureHandled(
      const content::DesktopMediaID& media_id,
      std::unique_ptr<TabDesktopMediaList::RefreshCompleter> refresh_completer,
      uint32_t new_hash,
      const gfx::ImageSkia& image);

  void StartPreviewUpdate();
  void TriggerScreenshot(
      int remaining_retries,
      std::unique_ptr<TabDesktopMediaList::RefreshCompleter> refresh_completer);
  void ScreenshotReceived(
      int remaining_retries,
      const content::DesktopMediaID& id,
      std::unique_ptr<TabDesktopMediaList::RefreshCompleter> refresh_completer,
      const SkBitmap& bitmap);
  void CompleteRefreshAfterThumbnailProcessing();

  // The WebContents from which the media-picker was invoked, if such
  // a WebContents was ever set.
  const std::optional<base::WeakPtr<content::WebContents>> web_contents_;

  // The hash of the last captured preview frame. Used to detect identical
  // frames and prevent needless rescaling.
  std::optional<uint32_t> last_hash_;

  ImageHashesMap favicon_hashes_;
  const DesktopMediaList::WebContentsFilter includable_web_contents_filter_;
  const bool include_chrome_app_windows_;

  // Task runner used for resizing thumbnail and preview images.
  scoped_refptr<base::SequencedTaskRunner> image_resize_task_runner_;

  std::optional<content::DesktopMediaID> previewed_source_;

  // Handle returned when incrementing the visible capturer count on the
  // WebContents instance being previewed, if there is one. Allowing this to go
  // out of scope, either when switching previewed sources or on destruction,
  // allows the source to stop painting when not visible etc.
  base::ScopedClosureRunner previewed_source_visible_keepalive_;

  base::WeakPtrFactory<TabDesktopMediaList> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_H_
