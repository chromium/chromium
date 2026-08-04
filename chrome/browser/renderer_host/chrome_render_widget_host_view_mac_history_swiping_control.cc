// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiping_control.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"

namespace history_swiper {

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistorySwipingControl);

HistorySwipingControl::HistorySwipingControl(
    content::WebContents* web_contents,
    base::RepeatingCallback<bool()> callback)
    : content::WebContentsUserData<HistorySwipingControl>(*web_contents),
      swiping_permission_callback_(std::move(callback)) {
  CHECK(swiping_permission_callback_);
}

HistorySwipingControl::~HistorySwipingControl() = default;

bool HistorySwipingControl::ShouldAllowHistorySwiping() const {
  return swiping_permission_callback_.Run();
}

}  // namespace history_swiper
