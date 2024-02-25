// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/text_fragment_lookup_state_tracker.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/companion/text_finder/text_finder.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"

namespace customtabs {

namespace {

const size_t kMaxNumLookupPerPage = 45;

// Notifies text fragment look up completion.
// `state_key` is opaque id used by client to keep track of the request.
// `lookup_results` is the mapping between the text fragments that were looked
// up and whether they were found on the page or not.
void NotifyCallback(
    TextFragmentLookupStateTracker::OnResultCallback cb,
    const std::string& state_key,
    const std::vector<std::pair<std::string, bool>>& lookup_results) {
  std::move(cb).Run(state_key, lookup_results);
}

}  // namespace

TextFragmentLookupStateTracker::~TextFragmentLookupStateTracker() = default;

TextFragmentLookupStateTracker::TextFragmentLookupStateTracker(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<TextFragmentLookupStateTracker>(
          *web_contents) {}

void TextFragmentLookupStateTracker::LookupTextFragment(
    const std::string& state_key,
    const std::vector<std::string>& text_directives,
    OnResultCallback on_result_callback) {
  const std::vector<std::string> allowed_text_directives =
      ExtractAllowedTextDirectives(text_directives);
  // Increment lookup counter.
  lookup_count_ += allowed_text_directives.size();
  DCHECK_LE(lookup_count_, kMaxNumLookupPerPage);

  // Create and attach a `TextFinderManager` to the primary page.
  content::Page& page = web_contents()->GetPrimaryPage();
  companion::TextFinderManager* text_finder_manager =
      companion::TextFinderManager::GetOrCreateForPage(page);
  DCHECK(text_finder_manager);

  companion::TextFinderManager::AllDoneCallback textfinder_finished_callback =
      base::BindOnce(&NotifyCallback, std::move(on_result_callback), state_key);

  text_finder_manager->CreateTextFinders(
      allowed_text_directives, std::move(textfinder_finished_callback));
}

std::vector<std::string>
TextFragmentLookupStateTracker::ExtractAllowedTextDirectives(
    const std::vector<std::string>& text_directives) const {
  // Check if the lookup counter exceeds the max number.
  if (lookup_count_ >= kMaxNumLookupPerPage) {
    return {};
  }

  // Extract the first allowed number of text directives.
  size_t cur_num = text_directives.size();
  if (lookup_count_ + cur_num <= kMaxNumLookupPerPage) {
    return text_directives;
  } else {
    // Throttled.
    size_t allowed_num = kMaxNumLookupPerPage - lookup_count_;
    return std::vector<std::string>(text_directives.begin(),
                                    text_directives.begin() + allowed_num);
  }
}

void TextFragmentLookupStateTracker::FindScrollAndHighlight(
    const std::string& text_directive) const {
  // Create and attach a `TextHighlighterManager` to the primary page.
  content::Page& page = web_contents()->GetPrimaryPage();
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetOrCreateForPage(page);
  DCHECK(text_highlighter_manager);

  text_highlighter_manager->CreateTextHighlighterAndRemoveExistingInstance(
      text_directive);
}

void TextFragmentLookupStateTracker::PrimaryPageChanged(content::Page& page) {
  // Reset lookup counter.
  lookup_count_ = 0;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TextFragmentLookupStateTracker);

}  // namespace customtabs
