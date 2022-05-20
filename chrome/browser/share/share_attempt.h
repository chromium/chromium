// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_ATTEMPT_H_
#define CHROME_BROWSER_SHARE_SHARE_ATTEMPT_H_

#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace share {

// A ShareAttempt represents a single attempt from the user to share a piece of
// content - a whole page, a link, an image, or perhaps (later on) other types.
struct ShareAttempt {
  explicit ShareAttempt(content::WebContents* contents);
  ShareAttempt(content::WebContents* contents, std::u16string title, GURL url);
  ~ShareAttempt();

  ShareAttempt(const ShareAttempt&);

  // TODO(https://crbug.com/1326249): It would be nice if this wasn't here.
  base::WeakPtr<content::WebContents> web_contents;
  const std::u16string title;
  const GURL url;
};

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_ATTEMPT_H_
