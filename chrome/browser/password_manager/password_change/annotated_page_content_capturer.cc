// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"

#include "content/public/browser/web_contents.h"

AnnotatedPageContentCapturer::AnnotatedPageContentCapturer(
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptionsPtr options,
    optimization_guide::OnAIPageContentDone callback)
    : content::WebContentsObserver(web_contents),
      options_(std::move(options)),
      callback_(std::move(callback)) {
  if (web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    DocumentOnLoadCompletedInPrimaryMainFrame();
  }
}

AnnotatedPageContentCapturer::~AnnotatedPageContentCapturer() = default;

void AnnotatedPageContentCapturer::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (callback_) {
    optimization_guide::GetAIPageContent(web_contents(), std::move(options_),
                                         std::move(callback_));
  }
}
