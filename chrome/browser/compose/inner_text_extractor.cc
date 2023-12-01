// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/inner_text_extractor.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"

#include "content/public/browser/web_contents.h"

InnerTextExtractor::InnerTextExtractor() : weak_ptr_factory_(this) {}

InnerTextExtractor::~InnerTextExtractor() = default;

void InnerTextExtractor::Extract(
    content::WebContents* web_contents,
    base::OnceCallback<void(const std::string&)> callback) {
  if (callbacks_.empty()) {
    callbacks_.push_back(std::move(callback));
    previous_web_contents_ = web_contents;
    content_extraction::GetInnerText(
        *web_contents->GetPrimaryMainFrame(), /*node_id*/ absl::nullopt,
        base::BindRepeating(&InnerTextExtractor::InnerTextCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (web_contents == previous_web_contents_) {
    callbacks_.push_back(std::move(callback));
  } else {
    DCHECK(false);
  }
}

void InnerTextExtractor::InnerTextCallback(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  std::string inner_text;
  if (result) {
    const compose::Config& config = compose::GetComposeConfig();
    inner_text = result->inner_text;
    compose::LogComposeDialogInnerTextSize(inner_text.size());
    if (inner_text.size() > config.inner_text_max_bytes) {
      compose::LogComposeDialogInnerTextShortenedBy(
          inner_text.size() - config.inner_text_max_bytes);
      // TODO(b/314230455): Reduce the number of times inner_text is copied.
      inner_text.erase(config.inner_text_max_bytes);
    }
  }
  for (auto& callback : callbacks_) {
    std::move(callback).Run(inner_text);
  }
  callbacks_.clear();
}
