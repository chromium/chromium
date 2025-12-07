// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

AnnotatedPageContentCapturer::AnnotatedPageContentCapturer(
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptionsPtr options,
    optimization_guide::OnAIPageContentDone callback)
    : AnnotatedPageContentCapturer(
          web_contents,
          std::move(options),
          std::move(callback),
          base::BindRepeating(&optimization_guide::GetAIPageContent,
                              base::Unretained(web_contents))) {}

AnnotatedPageContentCapturer::AnnotatedPageContentCapturer(
    base::PassKey<class AnnotatedPageContentCapturerTest>,
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptionsPtr options,
    optimization_guide::OnAIPageContentDone callback,
    GetAIPageContentFunction get_page_content)
    : AnnotatedPageContentCapturer(web_contents,
                                   std::move(options),
                                   std::move(callback),
                                   std::move(get_page_content)) {}

AnnotatedPageContentCapturer::AnnotatedPageContentCapturer(
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptionsPtr options,
    optimization_guide::OnAIPageContentDone callback,
    GetAIPageContentFunction get_page_content)
    : content::WebContentsObserver(web_contents),
      options_(std::move(options)),
      callback_(std::move(callback)),
      get_page_content_(std::move(get_page_content)) {
  if (!web_contents->IsLoading()) {
    DidStopLoading();
  }
}

AnnotatedPageContentCapturer::~AnnotatedPageContentCapturer() = default;

void AnnotatedPageContentCapturer::DidStopLoading() {
  if (web_contents()->IsLoading()) {
    // This event may be called before the page is ready. To prevent
    // capturing the annotated page content when the site is not fully loaded
    // we exit early if `IsLoading` is true.
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  get_page_content_.Run(
      options_->Clone(),
      base::BindOnce(&AnnotatedPageContentCapturer::CapturePageContent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AnnotatedPageContentCapturer::CapturePageContent(
    optimization_guide::AIPageContentResultOrError result) {
  if (callback_ && result.has_value() && result.value().proto.has_root_node()) {
    std::move(callback_).Run(std::move(result));
  }
}
