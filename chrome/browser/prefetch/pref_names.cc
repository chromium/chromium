// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/pref_names.h"

namespace prefetch {
namespace prefs {

// This pref contains a dictionary value whose keys are string representations
// of a URL. The values are a tuple (as a List Value) where the first value is a
// string representation of a URL and a a base::Time. This pref is limited to 10
// entries in the dictionary.
// The two URLs are not the same URL.
const char kCachePrefPath[] = "prefetch.search_prefetch.cache";

// This pref contains a dictionary value whose keys are string representations
// of a url::Origin and values are a base::Time. The recorded base::Time is the
// time at which prefetch requests to the corresponding origin can resume, (any
// base::Time that is in the past can be removed). Entries to the dictionary are
// created when a prefetch request gets a 503 response with Retry-After header.
const char kRetryAfterPrefPath[] =
    "chrome.prefetch_proxy.origin_decider.retry_after";

// This pref contains an integer (enum of type NetworkPredictionOptions),
// determining which preload pages mode (specifying whether or how much we are
// allowed to preload pages) has been selected by the user. Note that the name
// of this pref does not match the others in this file for legacy reasons.
const char kNetworkPredictionOptions[] = "net.network_prediction_options";

}  // namespace prefs
}  // namespace prefetch
