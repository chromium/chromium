// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_
#define CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_

#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Distills the current WebContents, waits for the result and continues to the
// Viewer via a navigation if successful. This does not create any web
// contents, or take ownership of the `web_contents` passed in.
void DistillCurrentPageAndViewIfSuccessful(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback);

// Starts distillation in the `source_web_contents`. The viewer needs to be
// created separately.
void DistillCurrentPage(content::WebContents* source_web_contents);

// Starts distillation in the `source_web_contents` while navigating the
// `destination_web_contents` to view the distilled content. This does not take
// ownership of any WebContents.
void DistillAndView(content::WebContents* source_web_contents,
                    content::WebContents* destination_web_contents);

// Runs the readability heuristic on the given `web_contents` to determine if
// it's suitable for reader mode. Returns the result through `callback`.
void RunReadabilityHeuristicsOnWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback);


#endif  // CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_
