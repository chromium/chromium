// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_
#define CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "url/gurl.h"

// Checks to see whether a given URL qualifies as a lookalike domain, and thus
// should trigger a safety tip. This algorithm factors in the sites that the
// user has already engaged with. This heuristic stores a "safe url" that the
// navigated domain is a lookalike to, in the passed |safe_url|.
//
// This heuristic should never be called with a URL which is already in
// |engaged_sites|.
bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    GURL* safe_url);

// Checks to see whether a given URL contains sensitive keywords in a way
// that it should trigger a safety tip.
//
// URLs without a TLD or with an unknown TLD never trigger.
bool ShouldTriggerSafetyTipFromKeywordInURL(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const char* const sensitive_keywords[],
    size_t num_keywords);

#endif  // CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_
