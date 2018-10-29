// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_

#include <string>

#include "base/macros.h"

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
                     long long doc_id,
                     long long snippet_hash);
  ~ResolvedSearchTerm();

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
  const long long doc_id;
  const long long snippet_hash;

  DISALLOW_COPY_AND_ASSIGN(ResolvedSearchTerm);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_RESOLVED_SEARCH_TERM_H_
