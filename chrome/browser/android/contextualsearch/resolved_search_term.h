// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_

#include <string.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <ostream>
#include <utility>

#include "base/bit_cast.h"
#include "base/containers/checked_iterators.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"

#include "base/macros.h"

// Provides the native support needed for the Java class by the same name,
// which encapsulates a server response for Contextual Search.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.contextualsearch
enum QuickActionCategory {
  QUICK_ACTION_CATEGORY_NONE,
  QUICK_ACTION_CATEGORY_ADDRESS,
  QUICK_ACTION_CATEGORY_EMAIL,
  QUICK_ACTION_CATEGORY_EVENT,
  QUICK_ACTION_CATEGORY_PHONE,
  QUICK_ACTION_CATEGORY_WEBSITE,
  QUICK_ACTION_CATEGORY_BOUNDARY
};

// Encapsulates the various parts of a Resolved Search Term, which tells
// Contextual Search what to search for and how that term appears in the
// surrounding text.
struct ResolvedSearchTerm {
 public:
  explicit ResolvedSearchTerm(int response_code);
  ResolvedSearchTerm(bool is_invalid,
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
                     int coca_card_tag,
                     const std::vector<std::string>& related_searches);
  ~ResolvedSearchTerm();

  // TODO(donnd): switch to member-initialization style instead of initializers.
  // TODO(donnd): change these members names to include an ending underscore.
  const bool is_invalid;
  const int response_code;
  // Use strings, rather than just references, to keep this complete.
  const std::string search_term;
  const std::string display_text;
  const std::string alternate_term;
  const std::string mid;  // Mid (entity ID), or empty.
  const bool prevent_preload;
  const int selection_start_adjust;
  const int selection_end_adjust;
  const std::string context_language;
  const std::string thumbnail_url;
  const std::string caption;
  const std::string quick_action_uri;
  const QuickActionCategory quick_action_category;
  const int64_t logged_event_id;  // Often 0.
  const std::string search_url_full;
  const std::string search_url_preload;
  const int coca_card_tag;
  std::vector<std::string> related_searches;

  DISALLOW_COPY_AND_ASSIGN(ResolvedSearchTerm);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_
