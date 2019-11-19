// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_H_

#include "chrome/browser/media/webrtc/desktop_media_list_base.h"

// Implementation of DesktopMediaList that shows tab/WebContents.
class TabDesktopMediaList : public DesktopMediaListBase {
 public:
  TabDesktopMediaList();
  ~TabDesktopMediaList() override;

 private:
  typedef std::map<content::DesktopMediaID, uint32_t> ImageHashesMap;

  void Refresh(bool update_thumnails) override;

  ImageHashesMap favicon_hashes_;

  // Task runner used for the |worker_|.
  scoped_refptr<base::SequencedTaskRunner> thumbnail_task_runner_;

  base::WeakPtrFactory<TabDesktopMediaList> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabDesktopMediaList);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_H_
