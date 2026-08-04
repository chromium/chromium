// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPING_CONTROL_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPING_CONTROL_H_

#include "base/functional/callback.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history_swiper {

// Tab helper attached to WebContents on macOS to dynamically allow embedder UIs
// (such as Profile Picker) to regulate trackpad history swiping gestures.
class HistorySwipingControl
    : public content::WebContentsUserData<HistorySwipingControl> {
 public:
  HistorySwipingControl(const HistorySwipingControl&) = delete;
  HistorySwipingControl& operator=(const HistorySwipingControl&) = delete;
  ~HistorySwipingControl() override;

  bool ShouldAllowHistorySwiping() const;

 private:
  HistorySwipingControl(content::WebContents* web_contents,
                        base::RepeatingCallback<bool()> callback);
  friend class content::WebContentsUserData<HistorySwipingControl>;

  base::RepeatingCallback<bool()> swiping_permission_callback_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace history_swiper

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPING_CONTROL_H_
