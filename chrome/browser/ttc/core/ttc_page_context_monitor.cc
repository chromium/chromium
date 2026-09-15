// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents.h"

namespace ttc {

TtcPageContextMonitor::TtcPageContextMonitor(
    content::WebContents& web_contents,
    PageChangedCallback page_changed_callback)
    : content::WebContentsObserver(&web_contents),
      page_changed_callback_(std::move(page_changed_callback)) {
  CHECK(page_changed_callback_);

  // TODO(bokan): We should consider whether now is a good time (e.g. are we
  // currently loading) and potentially defer this. Notify async to avoid
  // clients depending on this happening synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TtcPageContextMonitor::NotifyPageChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

TtcPageContextMonitor::~TtcPageContextMonitor() = default;

void TtcPageContextMonitor::StartNewFetch(FetchCompleteCallback callback) {
  fetcher_.reset();

  content::WebContents* contents = web_contents();
  if (!contents) {
    std::move(callback).Run(base::unexpected(
        page_content_annotations::FetchPageContextError::kWebContentsWentAway));
    return;
  }

  // No screenshot options are set below so the screenshot service is never
  // needed.
  fetcher_ = std::make_unique<page_content_annotations::PageContextFetcher>(
      /*get_screenshot_service_callback=*/base::NullCallback(),
      /*progress_listener=*/nullptr);

  page_content_annotations::FetchPageContextOptions options;
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  options.annotated_page_content_options->include_same_site_only = true;
  options.annotated_page_content_options->max_meta_elements = 32;

  fetcher_->FetchStart(
      *contents, options,
      base::BindOnce(&TtcPageContextMonitor::OnFetchComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TtcPageContextMonitor::PrimaryPageChanged(content::Page& page) {
  NotifyPageChanged();
}

void TtcPageContextMonitor::DidStopLoading() {
  NotifyPageChanged();
}

void TtcPageContextMonitor::NotifyPageChanged() {
  page_changed_callback_.Run();
}

void TtcPageContextMonitor::OnFetchComplete(
    FetchCompleteCallback callback,
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error().error_code));
    return;
  }

  page_content_annotations::FetchPageContextResult& fetch_result = **result;

  PageContext page_context;
  if (fetch_result.annotated_page_content_result.has_value()) {
    // Note: this slices off the PageContentResultWithEndTime members.
    optimization_guide::AIPageContentResult& ai_page_content =
        fetch_result.annotated_page_content_result.value();
    page_context.ai_page_content = std::move(ai_page_content);
  } else {
    page_context.ai_page_content = base::unexpected(result.error().error_code);
  }

  std::move(callback).Run(std::move(page_context));
}

}  // namespace ttc
