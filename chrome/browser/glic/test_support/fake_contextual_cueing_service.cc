// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/fake_contextual_cueing_service.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace glic {

namespace {
std::vector<std::string> GetSuggestionsForUrl(const GURL& url, int count) {
  std::vector<std::string> suggestions;
  for (int i = 1; i <= count; i++) {
    suggestions.push_back(base::StrCat(
        {"Sug ", base::NumberToString(i), " for ", url.PathForRequestPiece()}));
  }
  return suggestions;
}
}  // namespace

FakeContextualCueingService::FakeContextualCueingService()
    : ContextualCueingService(
          /*page_content_extraction_service=*/nullptr,
          /*optimization_guide_keyed_service=*/nullptr,
          /*loading_predictor=*/nullptr,
          /*identity_manager=*/nullptr,
          /*pref_service=*/nullptr,
          /*template_url_service=*/nullptr) {}

FakeContextualCueingService::~FakeContextualCueingService() = default;

void FakeContextualCueingService::
    GetContextualGlicZeroStateSuggestionsForFocusedTab(
        content::WebContents* web_contents,
        bool is_fre,
        std::optional<std::vector<std::string>> supported_tools,
        GlicSuggestionsCallback callback) {
  if (!IsZeroStateSuggestionsEnabled()) {
    std::move(callback).Run({});
    return;
  }
  focused_tab_call_count_++;
  std::vector<std::string> suggestions;
  if (!focused_tab_suggestions_ && web_contents) {
    suggestions = GetSuggestionsForUrl(web_contents->GetLastCommittedURL(), 3);
  } else if (focused_tab_suggestions_) {
    suggestions = *focused_tab_suggestions_;
  }

  focused_tab_callbacks_.push_back(std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeContextualCueingService::ProvideFocusedTabSuggestions,
                     base::Unretained(this), suggestions));
}

bool FakeContextualCueingService::
    GetContextualGlicZeroStateSuggestionsForPinnedTabs(
        std::vector<content::WebContents*> pinned_web_contents,
        bool is_fre,
        std::optional<std::vector<std::string>> supported_tools,
        const content::WebContents* focused_tab,
        GlicSuggestionsCallback callback) {
  if (!IsZeroStateSuggestionsEnabled()) {
    std::move(callback).Run({});
    return false;
  }
  pinned_tabs_call_count_++;
  std::vector<std::string> suggestions;
  if (!pinned_tabs_suggestions_ && focused_tab) {
    suggestions = GetSuggestionsForUrl(focused_tab->GetLastCommittedURL(), 3);
  } else if (!pinned_tabs_suggestions_ && !pinned_web_contents.empty()) {
    suggestions =
        GetSuggestionsForUrl(pinned_web_contents[0]->GetLastCommittedURL(), 3);
  } else if (pinned_tabs_suggestions_) {
    suggestions = *pinned_tabs_suggestions_;
  }

  pinned_tabs_callbacks_.push_back(std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeContextualCueingService::ProvidePinnedTabsSuggestions,
                     base::Unretained(this), suggestions));
  return true;
}

void FakeContextualCueingService::ProvideFocusedTabSuggestions(
    const std::vector<std::string>& suggestions) {
  auto callbacks = std::exchange(focused_tab_callbacks_, {});
  for (auto& callback : callbacks) {
    if (!callback.is_null()) {
      std::move(callback).Run(suggestions);
    }
  }
}

void FakeContextualCueingService::ProvidePinnedTabsSuggestions(
    const std::vector<std::string>& suggestions) {
  auto callbacks = std::exchange(pinned_tabs_callbacks_, {});
  for (auto& callback : callbacks) {
    if (!callback.is_null()) {
      std::move(callback).Run(suggestions);
    }
  }
}

}  // namespace glic
