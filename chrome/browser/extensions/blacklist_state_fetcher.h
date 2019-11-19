// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/db/util.h"
#include "extensions/browser/blacklist_state.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace extensions {
class TestBlacklistStateFetcher;

class BlacklistStateFetcher {
 public:
  typedef base::Callback<void(BlacklistState)> RequestCallback;

  BlacklistStateFetcher();

  virtual ~BlacklistStateFetcher();

  virtual void Request(const std::string& id, const RequestCallback& callback);

  void SetSafeBrowsingConfig(const safe_browsing::V4ProtocolConfig& config);

 protected:
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::unique_ptr<std::string> response_body);

  // Used for ease unit tests.
  void OnURLLoaderCompleteInternal(network::SimpleURLLoader* url_loader,
                                   const std::string& response_body,
                                   int response_code,
                                   int net_error);

 private:
  friend class TestBlacklistStateFetcher;
  typedef std::multimap<std::string, RequestCallback> CallbackMultiMap;

  GURL RequestUrl() const;

  void SendRequest(const std::string& id);

  std::unique_ptr<safe_browsing::V4ProtocolConfig> safe_browsing_config_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // SimpleURLLoader -> (owned loader, extension id).
  std::map<const network::SimpleURLLoader*,
           std::pair<std::unique_ptr<network::SimpleURLLoader>, std::string>>
      requests_;

  // Callbacks by extension ID.
  CallbackMultiMap callbacks_;

  base::WeakPtrFactory<BlacklistStateFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BlacklistStateFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_STATE_FETCHER_H_
