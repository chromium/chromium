// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_FETCHER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_FETCHER_WIN_H_

#include "base/callback.h"

class PrefService;

namespace base {
class FilePath;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace safe_browsing {

enum class ChromeCleanerFetchStatus {
  // Fetch succeeded with a net::HTTP_OK response code.
  kSuccess,
  // File system error, no fetch on the network was attempted.
  kFailedToCreateTemporaryDirectory,
  // We received a net::HTTP_NOT_FOUND response code.
  kNotFoundOnServer,
  // Fetch failed or we received an http response code other than net::HTTP_OK
  // and net::HTTP_NOT_FOUND.
  kOtherFailure,
};

// Type of callback that is called when the network request to fetch the Chrome
// Cleaner binary has been completed.
using ChromeCleanerFetchedCallback =
    base::OnceCallback<void(base::FilePath,
                            ChromeCleanerFetchStatus fetch_status)>;

// Fetches the Chrome Cleaner binary. This function can be called from any
// sequence and |fetched_callback| will be called back on that same sequence.
void FetchChromeCleaner(ChromeCleanerFetchedCallback fetched_callback,
                        network::mojom::URLLoaderFactory* url_loader_factory,
                        PrefService* prefs);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_FETCHER_WIN_H_
