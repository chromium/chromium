// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace {

optimization_guide::AIPageContentResultOrError LogPageContentCaptureMetrics(
    base::TimeTicks start_time,
    optimization_guide::AIPageContentResultOrError result) {
  base::UmaHistogramMediumTimes(
      "PasswordManager.PasswordChange.PageContentCaptureDuration",
      base::TimeTicks::Now() - start_time);
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordChange.PageContentCaptureResult",
      result.has_value());
  return result;
}

}  // namespace

AnnotatedPageContentCapturerImpl::AnnotatedPageContentCapturerImpl(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    blink::mojom::AIPageContentOptionsPtr options,
    optimization_guide::OnAIPageContentDone callback,
    AnnotatedPageContentCapturer::GetAIPageContentFunction get_page_content)
    : content::WebContentsObserver(web_contents),
      options_(std::move(options)),
      callback_(
          base::BindOnce(&LogPageContentCaptureMetrics, base::TimeTicks::Now())
              .Then(std::move(callback))),
      get_page_content_(std::move(get_page_content)) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kAwaitPageStabilityForPasswordChange)) {
    page_stability_waiter_ =
        std::make_unique<PasswordChangePageStabilityWaiter>(
            web_contents, client,
            base::BindOnce(&AnnotatedPageContentCapturerImpl::OnPageStable,
                           weak_ptr_factory_.GetWeakPtr()));
  } else {
    if (!web_contents->IsLoading()) {
      DidStopLoading();
    }
  }
}

AnnotatedPageContentCapturerImpl::~AnnotatedPageContentCapturerImpl() = default;

void AnnotatedPageContentCapturerImpl::DidStopLoading() {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kAwaitPageStabilityForPasswordChange)) {
    return;
  }
  if (web_contents()->IsLoading()) {
    return;
  }
  OnPageStable();
}

void AnnotatedPageContentCapturerImpl::OnPageStable() {
  page_stability_waiter_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  get_page_content_.Run(
      options_->Clone(),
      base::BindOnce(&AnnotatedPageContentCapturerImpl::CapturePageContent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AnnotatedPageContentCapturerImpl::CapturePageContent(
    optimization_guide::AIPageContentResultOrError result) {
  if (result.has_value() ||
      !base::FeatureList::IsEnabled(
          password_manager::features::kRetryCapturePageContent) ||
      retry_count_ >=
          password_manager::features::kCapturePageContentRetryCount.Get()) {
    if (callback_) {
      std::move(callback_).Run(std::move(result));
    }
    return;
  }

  retry_count_++;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AnnotatedPageContentCapturerImpl::RetryCapture,
                     weak_ptr_factory_.GetWeakPtr()),
      password_manager::features::kCapturePageContentDelay.Get());
}

void AnnotatedPageContentCapturerImpl::RetryCapture() {
  get_page_content_.Run(
      options_->Clone(),
      base::BindOnce(&AnnotatedPageContentCapturerImpl::CapturePageContent,
                     weak_ptr_factory_.GetWeakPtr()));
}
