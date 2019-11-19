// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_NATIVE_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_NATIVE_DESKTOP_MEDIA_LIST_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/media/webrtc/desktop_media_list_base.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/gfx/image/image.h"

namespace webrtc {
class DesktopCapturer;
}

// Implementation of DesktopMediaList that shows native screens and
// native windows.
class NativeDesktopMediaList : public DesktopMediaListBase {
 public:
  NativeDesktopMediaList(content::DesktopMediaID::Type type,
                         std::unique_ptr<webrtc::DesktopCapturer> capturer);
  ~NativeDesktopMediaList() override;

 private:
  typedef std::map<content::DesktopMediaID, uint32_t> ImageHashesMap;

  class Worker;
  friend class Worker;

  // Refresh() posts a task for the |worker_| to update list of windows, get
  // thumbnails and schedules next refresh.
  void Refresh(bool update_thumnails) override;

  void RefreshForAuraWindows(std::vector<SourceDescription> sources,
                             bool update_thumnails);
  void UpdateNativeThumbnailsFinished();

#if defined(USE_AURA)
  void CaptureAuraWindowThumbnail(const content::DesktopMediaID& id);
  void OnAuraThumbnailCaptured(const content::DesktopMediaID& id,
                               gfx::Image image);
#endif

  base::Thread thread_;
  std::unique_ptr<Worker> worker_;

#if defined(USE_AURA)
  // previous_aura_thumbnail_hashes_ holds thumbanil hash values of aura windows
  // in the previous refresh. While new_aura_thumbnail_hashes_ has hash values
  // of the ongoing refresh. Those two maps are used to detect new thumbnails
  // and changed thumbnails from the previous refresh.
  ImageHashesMap previous_aura_thumbnail_hashes_;
  ImageHashesMap new_aura_thumbnail_hashes_;

  int pending_aura_capture_requests_ = 0;
  bool pending_native_thumbnail_capture_ = false;
#endif
  base::WeakPtrFactory<NativeDesktopMediaList> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NativeDesktopMediaList);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_NATIVE_DESKTOP_MEDIA_LIST_H_
