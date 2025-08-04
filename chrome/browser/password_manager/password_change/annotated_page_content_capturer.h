// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_

#include "base/functional/callback_forward.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents_observer.h"

// Helper class which captures annotated page content. Waits for page loading to
// finish if necessary.
class AnnotatedPageContentCapturer : public content::WebContentsObserver {
 public:
  AnnotatedPageContentCapturer(
      content::WebContents* web_contents,
      blink::mojom::AIPageContentOptionsPtr options,
      optimization_guide::OnAIPageContentDone callback);

  ~AnnotatedPageContentCapturer() override;

  // content::WebContentsObserver
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

#if defined(UNIT_TEST)
  void ReplyWithContent(
      std::optional<optimization_guide::AIPageContentResult> result) {
    std::move(callback_).Run(std::move(result));
  }
#endif

 private:
  blink::mojom::AIPageContentOptionsPtr options_;
  optimization_guide::OnAIPageContentDone callback_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_
