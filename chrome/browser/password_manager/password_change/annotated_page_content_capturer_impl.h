// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class PasswordChangePageStabilityWaiter;

class AnnotatedPageContentCapturerImpl : public AnnotatedPageContentCapturer,
                                         public content::WebContentsObserver {
 public:
  AnnotatedPageContentCapturerImpl(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      blink::mojom::AIPageContentOptionsPtr options,
      optimization_guide::OnAIPageContentDone callback,
      AnnotatedPageContentCapturer::GetAIPageContentFunction get_page_content);

  ~AnnotatedPageContentCapturerImpl() override;

  // content::WebContentsObserver
  void DidStopLoading() override;

  void OnPageStable();

 private:
  void CapturePageContent(
      optimization_guide::AIPageContentResultOrError result);
  void RetryCapture();

  blink::mojom::AIPageContentOptionsPtr options_;
  optimization_guide::OnAIPageContentDone callback_;
  AnnotatedPageContentCapturer::GetAIPageContentFunction get_page_content_;
  std::unique_ptr<PasswordChangePageStabilityWaiter> page_stability_waiter_;

  int retry_count_ = 0;

  base::WeakPtrFactory<AnnotatedPageContentCapturerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_IMPL_H_
