// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/guest_util.h"

#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace glic {

void OnGuestAdded(content::WebContents* guest_contents) {
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);

  if (top && top->GetLastCommittedURL() == GURL(chrome::kChromeUIGlicURL)) {
    // TODO(crbug.com/382322927): This could instead be done by having all guest
    // WebContents inherit background color from their embedders.
    guest_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  }
}

}  // namespace glic
