// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_ASH_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_ASH_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/webrtc/desktop_media_list_base.h"
#include "content/public/browser/desktop_media_id.h"

namespace aura {
class Window;
}

namespace gfx {
class Image;
}

// Implementation of DesktopMediaList that shows native screens and
// native windows.
class DesktopMediaListAsh : public DesktopMediaListBase {
 public:
  explicit DesktopMediaListAsh(content::DesktopMediaID::Type type);
  ~DesktopMediaListAsh() override;

 private:
  // Override from DesktopMediaListBase.
  void Refresh(bool update_thumnails) override;
  void EnumerateWindowsForRoot(
      std::vector<DesktopMediaListAsh::SourceDescription>* windows,
      bool update_thumnails,
      aura::Window* root_window,
      int container_id);
  void EnumerateSources(
      std::vector<DesktopMediaListAsh::SourceDescription>* windows,
      bool update_thumnails);
  void CaptureThumbnail(content::DesktopMediaID id, aura::Window* window);
  void OnThumbnailCaptured(content::DesktopMediaID id, gfx::Image image);
  void OnRefreshMaybeComplete();

  int pending_window_capture_requests_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DesktopMediaListAsh> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaListAsh);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_ASH_H_
