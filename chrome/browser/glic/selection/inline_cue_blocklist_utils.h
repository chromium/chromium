// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_INLINE_CUE_BLOCKLIST_UTILS_H_
#define CHROME_BROWSER_GLIC_SELECTION_INLINE_CUE_BLOCKLIST_UTILS_H_

#include <string>
#include <vector>

class GURL;
class Profile;

namespace glic {

// Returns true if the inline cue is blocked on `url`.
//
// Precedence, highest first:
//   1. The global toggle. Turning the inline cue off blocks every site.
//   2. A per-site rule the user set. CONTENT_SETTING_BLOCK means the user
//      blocked the site. CONTENT_SETTING_ALLOW means the user removed a
//      default-blocked site from the list; it is recorded so that the removal
//      survives restarts, because the default blocklist is a server-provided
//      parameter rather than persisted state.
//   3. The server-provided default blocklist.
//
// TODO(b/533390386): CONTENT_SETTING_ALLOW is overloaded as a temporary
// solution: it records an opt-out from the default blocklist rather than a
// standalone user preference.
bool IsSiteBlockedForInlineCue(Profile* profile, const GURL& url);

// Returns true if the `pattern_str` matches any site in the default blocklist
// for the inline cue.
bool IsSiteInDefaultBlocklistForInlineCue(const std::string& pattern_str);

// Returns the list of site patterns from the default blocklist that are not
// overridden by a user rule for the inline cue.
std::vector<std::string> GetActiveDefaultBlockedSitePatternsForInlineCue(
    Profile* profile);

// If `pattern_str` is in the default blocklist for the inline cue, unblocks it
// by setting CONTENT_SETTING_ALLOW in HostContentSettingsMap, and returns true.
// Returns false if `pattern_str` is not in the default blocklist.
bool UnblockDefaultSiteForInlineCue(Profile* profile,
                                    const std::string& pattern_str);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_INLINE_CUE_BLOCKLIST_UTILS_H_
