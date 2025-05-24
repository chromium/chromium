// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PERFORM_NETWORK_CONTEXT_PREFETCH_H_
#define CHROME_BROWSER_PREDICTORS_PERFORM_NETWORK_CONTEXT_PREFETCH_H_

#include <vector>

class GURL;
class Profile;

namespace predictors {

struct PrefetchRequest;

// Use the NetworkContext::Prefetch() API to perform a prefetch inside the
// network service that may be directly inherited by a render process. `page` is
// the top frame page for the navigation. `requests` contains a list of the
// subresources to be prefetched. Generates a network::ResourceRequest object
// for each PrefetchRequest with the fields filled in to match as closely as
// possible what the render process is expected to generate for the same
// resource later. Does not perform prefetches for incognito profiles.
void PerformNetworkContextPrefetch(Profile* profile,
                                   const GURL& page,
                                   std::vector<PrefetchRequest> requests);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PERFORM_NETWORK_CONTEXT_PREFETCH_H_
