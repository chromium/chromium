// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/resolved_search_term.h"

#include "net/url_request/url_fetcher.h"

ResolvedSearchTerm::ResolvedSearchTerm(int response_code)
    : is_invalid(response_code == net::URLFetcher::RESPONSE_CODE_INVALID),
      response_code(response_code),
      search_term(""),
      display_text(""),
      alternate_term(""),
      mid(""),
      prevent_preload(false),
      selection_start_adjust(0),
      selection_end_adjust(0),
      context_language(""),
      thumbnail_url(""),
      caption(""),
      quick_action_uri(""),
      quick_action_category(QUICK_ACTION_CATEGORY_NONE),
      logged_event_id(0),
      search_url_full(""),
      search_url_preload(""),
      coca_card_tag(0) {}

ResolvedSearchTerm::ResolvedSearchTerm(
    bool is_invalid,
    int response_code,
    const std::string& search_term,
    const std::string& display_text,
    const std::string& alternate_term,
    const std::string& mid,
    bool prevent_preload,
    int selection_start_adjust,
    int selection_end_adjust,
    const std::string& context_language,
    const std::string& thumbnail_url,
    const std::string& caption,
    const std::string& quick_action_uri,
    const QuickActionCategory& quick_action_category,
    int64_t logged_event_id,
    const std::string& search_url_full,
    const std::string& search_url_preload,
    int coca_card_tag)
    : is_invalid(is_invalid),
      response_code(response_code),
      search_term(search_term),
      display_text(display_text),
      alternate_term(alternate_term),
      mid(mid),
      prevent_preload(prevent_preload),
      selection_start_adjust(selection_start_adjust),
      selection_end_adjust(selection_end_adjust),
      context_language(context_language),
      thumbnail_url(thumbnail_url),
      caption(caption),
      quick_action_uri(quick_action_uri),
      quick_action_category(quick_action_category),
      logged_event_id(logged_event_id),
      search_url_full(search_url_full),
      search_url_preload(search_url_preload),
      coca_card_tag(coca_card_tag) {}

ResolvedSearchTerm::~ResolvedSearchTerm() {}
