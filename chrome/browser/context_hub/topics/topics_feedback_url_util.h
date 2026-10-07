// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPICS_FEEDBACK_URL_UTIL_H_
#define CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPICS_FEEDBACK_URL_UTIL_H_

#include <optional>
#include <string_view>

#include "url/gurl.h"

namespace context_hub {

// Value substituted for secret query parameter values by
// `MinimizeUrlForTopicsFeedback()`.
inline constexpr char kTopicsFeedbackRedactedValue[] = "REDACTED";

// Returns `url` reduced to what the Topics fishfood feedback export may
// contain, or `std::nullopt` if the visit must be dropped from the export
// entirely. Specifically:
//  - Drops (returns nullopt for) invalid URLs and any scheme other than
//    http/https, e.g. file:, data:, chrome:, chrome-extension:, about:.
//  - Removes the username, password, and fragment.
//  - Replaces the value of well-known secret query parameters (matched
//    case-insensitively by name, e.g. `token`, `api_key`, `session_id`) with
//    `kTopicsFeedbackRedactedValue`. All other query parameters, including
//    their order and encoding, are preserved.
std::optional<GURL> MinimizeUrlForTopicsFeedback(const GURL& url);

// Returns true if visits to `host` are excluded from the export by default,
// i.e. internal Google hosts (`*.corp.google.com`, `*.googleplex.com`,
// `goto.google.com`) and Google Workspace surfaces (Docs, Drive, Gmail, Chat,
// Meet, Calendar, Colab). Raters can override this per domain. `host` must be
// canonical, e.g. `GURL::host()`; a trailing dot is ignored.
bool IsDefaultExcludedDomain(std::string_view host);

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPICS_FEEDBACK_URL_UTIL_H_
