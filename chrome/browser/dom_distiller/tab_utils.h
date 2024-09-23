// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_
#define CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_

namespace content {
class WebContents;
}  // namespace content

// Creates a new WebContents and navigates it to view the URL of the current
// page, while in the background starts distilling the current page. This method
// takes ownership over the old WebContents after swapping in the new one.
void DistillCurrentPageAndView(content::WebContents* old_web_contents);

// Starts distillation in the |source_web_contents|. The viewer needs to be
// created separately.
void DistillCurrentPage(content::WebContents* source_web_contents);

// Starts distillation in the |source_web_contents| while navigating the
// |destination_web_contents| to view the distilled content. This does not take
// ownership of any WebContents.
void DistillAndView(content::WebContents* source_web_contents,
                    content::WebContents* destination_web_contents);

#endif  // CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_
