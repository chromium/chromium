// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_ATTEMPT_H_
#define CHROME_BROWSER_SHARE_SHARE_ATTEMPT_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace share {

// A ShareAttempt represents a single attempt from the user to share a piece of
// content - a whole page, a link, an image, or perhaps (later on) other types.
struct ShareAttempt {
  explicit ShareAttempt(content::WebContents* contents);
  ShareAttempt(content::WebContents* contents,
               std::u16string title,
               GURL url,
               ui::ImageModel preview_image);
  ~ShareAttempt();

  ShareAttempt(const ShareAttempt&);

  // TODO(crbug.com/40840434): It would be nice if this wasn't here.
  base::WeakPtr<content::WebContents> web_contents;
  const std::u16string title;
  const GURL url;

  // The initial preview image to use for the share - note that this may get
  // replaced if a better preview image becomes available. See
  // SharingHubBubbleController::RegisterPreviewImageChangedCallback().
  ui::ImageModel preview_image;
};

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_ATTEMPT_H_
