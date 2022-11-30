// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_
#define CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_

#include <vector>

#include "components/lookalikes/core/lookalike_url_util.h"
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

#endif  // CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_
