// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/sessions/content/session_tab_helper.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace glic {

glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SkBitmap favicon;
  auto* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  if (favicon_driver) {
    if (favicon_driver->FaviconIsValid()) {
      favicon = favicon_driver->GetFavicon().AsBitmap();
    }
  }
  return glic::mojom::TabData::New(
      sessions::SessionTabHelper::IdForTab(web_contents).id(),
      sessions::SessionTabHelper::IdForWindowContainingTab(web_contents).id(),
      web_contents->GetLastCommittedURL(),
      base::UTF16ToUTF8(web_contents->GetTitle()), favicon,
      web_contents->GetContentsMimeType());
}

}  // namespace glic
