// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FAKE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FAKE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_

#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

class FakeAnnotatedPageContentCapturer : public AnnotatedPageContentCapturer {
 public:
  explicit FakeAnnotatedPageContentCapturer(
      optimization_guide::OnAIPageContentDone callback);

  ~FakeAnnotatedPageContentCapturer() override;

  void SimulateResponse(optimization_guide::AIPageContentResultOrError result) {
    if (callback_) {
      std::move(callback_).Run(std::move(result));
    }
  }

 private:
  optimization_guide::OnAIPageContentDone callback_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FAKE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_
