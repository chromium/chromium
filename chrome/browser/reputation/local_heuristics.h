// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_
#define CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_

#include <vector>

#include "components/lookalikes/core/lookalike_url_util.h"
#include "url/gurl.h"

// Returns true if a given URL qualifies as a lookalike domain, and thus
// should show a safety tip warning. This check factors in the sites that
// the user has already engaged with. This function stores a "safe url" that
// the navigated domain is a lookalike to, in the passed |safe_url|.
//
// This function should never be called with a URL which is already in
// |engaged_sites|.
bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    GURL* safe_url,
    LookalikeUrlMatchType* match_type);

#endif  // CHROME_BROWSER_REPUTATION_LOCAL_HEURISTICS_H_
