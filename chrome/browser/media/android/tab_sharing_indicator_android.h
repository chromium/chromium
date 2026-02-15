// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_TAB_SHARING_INDICATOR_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_TAB_SHARING_INDICATOR_ANDROID_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents_observer.h"

// Android-specific implementation of MediaStreamUI for tab sharing state
// (capturee).
class TabSharingIndicatorAndroid : public MediaStreamUI {
 public:
  explicit TabSharingIndicatorAndroid(const content::DesktopMediaID& media_id);
  ~TabSharingIndicatorAndroid() override;

  // chrome::MediaStreamUI override.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

 private:
  void StopSharing();

  base::OnceClosure stop_callback_;
  const content::DesktopMediaID media_id_;
  std::unique_ptr<content::MediaStreamUI> tab_sharing_indicator_ui_;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_TAB_SHARING_INDICATOR_ANDROID_H_
