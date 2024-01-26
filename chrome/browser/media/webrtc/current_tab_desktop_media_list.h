// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CURRENT_TAB_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CURRENT_TAB_DESKTOP_MEDIA_LIST_H_

#include "chrome/browser/media/webrtc/desktop_media_list_base.h"

#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_contents.h"

// Implementation of DesktopMediaList that only follows a single tab.
class CurrentTabDesktopMediaList : public DesktopMediaListBase {
 public:
  explicit CurrentTabDesktopMediaList(content::WebContents* web_contents);

  CurrentTabDesktopMediaList(const CurrentTabDesktopMediaList&) = delete;
  CurrentTabDesktopMediaList& operator=(const CurrentTabDesktopMediaList&) =
      delete;

  ~CurrentTabDesktopMediaList() override;

 private:
  friend class CurrentTabDesktopMediaListTest;

  // For testing.
  CurrentTabDesktopMediaList(content::WebContents* web_contents,
                             base::TimeDelta period,
                             DesktopMediaListObserver* observer);

  void Refresh(bool update_thumbnails) override;

  // Called on the UI thread after the captured image is handled. If the
  // image was new, it's rescaled to the desired size and sent back in |image|.
  // Otherwise, an empty Optional is sent back. In either case, |hash| is the
  // hash value of the frame that was handled.
  void OnCaptureHandled(uint32_t hash,
                        const std::optional<gfx::ImageSkia>& image);

  // It's hard for tests to control what kind of image is given by the mock.
  // Normally it just returns the same thing again and again. To simulate a
  // new frame, we can just forget the last one.
  void ResetLastHashForTesting();

  // This "list" tracks a single view - the one represented by these variables.
  const content::DesktopMediaID media_id_;

  // Avoid two concurrent refreshes.
  bool refresh_in_progress_ = false;

  // The hash of the last captured frame. Used to detect identical frames
  // and prevent needless rescaling.
  std::optional<uint32_t> last_hash_;

  // The heavy lifting involved with rescaling images into thumbnails is
  // moved off of the UI thread and onto this task runner.
  scoped_refptr<base::SequencedTaskRunner> thumbnail_task_runner_;

  base::WeakPtrFactory<CurrentTabDesktopMediaList> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CURRENT_TAB_DESKTOP_MEDIA_LIST_H_
