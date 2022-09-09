// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_BLOCKLIST_STATE_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_BLOCKLIST_STATE_FETCHER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/blocklist_state_fetcher.h"
#include "chrome/common/safe_browsing/crx_info.pb.h"

namespace extensions {

// A wrapper for extensions::BlocklistStateFetcher, emulating server responses.
class TestBlocklistStateFetcher {
 public:
  explicit TestBlocklistStateFetcher(BlocklistStateFetcher* fetcher);

  TestBlocklistStateFetcher(const TestBlocklistStateFetcher&) = delete;
  TestBlocklistStateFetcher& operator=(const TestBlocklistStateFetcher&) =
      delete;

  ~TestBlocklistStateFetcher();

  void SetBlocklistVerdict(const std::string& id,
                           ClientCRXListInfoResponse_Verdict state);

  // Send the appropriate response for the request for extension with id |id|.
  // Return false, if fetcher with fiven id doesn't exist or in case of
  // incorrect request. Otherwise return true.
  bool HandleFetcher(const std::string& id);

 private:
  raw_ptr<BlocklistStateFetcher> fetcher_;

  std::map<std::string, ClientCRXListInfoResponse_Verdict> verdicts_;

  // Dummy URLLoaderFactory not used for responses but avoids crashes.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_BLOCKLIST_STATE_FETCHER_H_
