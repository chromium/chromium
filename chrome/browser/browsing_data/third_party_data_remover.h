// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_THIRD_PARTY_DATA_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_THIRD_PARTY_DATA_REMOVER_H_

#include "base/callback.h"
#include "content/public/browser/browser_context.h"

// This function clears cookies available in cross-site contexts (i.e.
// SameSite=None cookies) and clears any storage that was accessed in cross-site
// contexts if access context auditing is enabled. If access context auditing is
// disabled, the function will clear storage for any origin whose domain has a
// SameSite=None cookie.
void ClearThirdPartyData(base::OnceClosure closure,
                         content::BrowserContext* context);

#endif  // CHROME_BROWSER_BROWSING_DATA_THIRD_PARTY_DATA_REMOVER_H_
