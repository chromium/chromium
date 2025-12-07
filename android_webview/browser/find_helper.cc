// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/find_helper.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

using content::WebContents;

namespace android_webview {

FindHelper::FindHelper(WebContents* web_contents)
    : web_contents_(web_contents) {}

FindHelper::~FindHelper() {
}

void FindHelper::SetListener(Listener* listener) {
  listener_ = listener;
}

void FindHelper::FindAllAsync(const std::u16string& search_string) {
  // Stop any ongoing asynchronous request.
  web_contents_->StopFinding(content::STOP_FIND_ACTION_KEEP_SELECTION);

  async_find_started_ = true;

  StartNewSession(search_string);

  if (MaybeHandleEmptySearch(search_string))
    return;

  auto options = blink::mojom::FindOptions::New();
  options->forward = true;
  options->match_case = false;
  options->new_session = true;

  web_contents_->Find(current_request_id_, search_string, std::move(options),
                      /*skip_delay=*/false);
}

void FindHelper::HandleFindReply(int request_id,
                                 int match_count,
                                 int active_ordinal,
                                 bool finished) {
  if (!async_find_started_ || request_id < current_session_id_)
    return;

  NotifyResults(active_ordinal, match_count, finished);
}

void FindHelper::FindNext(bool forward) {
  if (!async_find_started_)
    return;

  current_request_id_ = find_request_id_counter_++;

  if (MaybeHandleEmptySearch(last_search_string_))
    return;

  auto options = blink::mojom::FindOptions::New();
  options->forward = forward;
  options->match_case = false;
  options->new_session = false;

  web_contents_->Find(current_request_id_, last_search_string_,
                      std::move(options), /*skip_delay=*/false);
}

void FindHelper::ClearMatches() {
  web_contents_->StopFinding(content::STOP_FIND_ACTION_CLEAR_SELECTION);

  async_find_started_ = false;
  last_search_string_.clear();
  last_match_count_ = -1;
  last_active_ordinal_ = -1;
}

bool FindHelper::MaybeHandleEmptySearch(const std::u16string& search_string) {
  if (!search_string.empty())
    return false;

  web_contents_->StopFinding(content::STOP_FIND_ACTION_CLEAR_SELECTION);
  NotifyResults(0, 0, true);
  return true;
}

void FindHelper::StartNewSession(const std::u16string& search_string) {
  current_request_id_ = find_request_id_counter_++;
  current_session_id_ = current_request_id_;
  last_search_string_ = search_string;
  last_match_count_ = -1;
  last_active_ordinal_ = -1;
}

void FindHelper::NotifyResults(int active_ordinal,
                               int match_count,
                               bool finished) {
  // Match count or ordinal values set to -1 refer to received replies.
  if (match_count == -1)
    match_count = last_match_count_;
  else
    last_match_count_ = match_count;

  if (active_ordinal == -1)
    active_ordinal = last_active_ordinal_;
  else
    last_active_ordinal_ = active_ordinal;

  // Skip the update if we don't still have a valid ordinal.
  // The next update, final or not, should have this information.
  if (!finished && active_ordinal == -1)
    return;

  // Safeguard in case of errors to prevent reporting -1 to the API listeners.
  if (match_count == -1) {
    NOTREACHED();
  }

  if (active_ordinal == -1) {
    NOTREACHED();
  }

  // WebView.FindListener active match ordinals are 0-based while WebKit sends
  // 1-based ordinals. Still we can receive 0 ordinal in case of no results.
  active_ordinal = std::max(active_ordinal - 1, 0);

  if (listener_)
    listener_->OnFindResultReceived(active_ordinal, match_count, finished);
}

}  // namespace android_webview
