// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_SOCS_COOKIE_FETCHER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_SOCS_COOKIE_FETCHER_H_

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace app_list {

// This class is responsible for fetching SOCS cookie from chromeoscompliance
// API and pass it to its consumer after the request is completed.
// SocsCookieFetcher is still WIP.
class SocsCookieFetcher final {
 public:
  class Consumer {
   public:
    Consumer();
    virtual ~Consumer();
    virtual void OnCookieFetched(const std::string& cookie_header) = 0;
  };

  SocsCookieFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Consumer* consumer);
  ~SocsCookieFetcher();

  // Disallow copy and assign.
  SocsCookieFetcher(const SocsCookieFetcher&) = delete;
  SocsCookieFetcher& operator=(const SocsCookieFetcher&) = delete;

  void StartFetching();

 private:
  // `consumer_` to call back when this request completes.
  const raw_ptr<Consumer, ExperimentalAsh> consumer_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<SocsCookieFetcher> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_SOCS_COOKIE_FETCHER_H_
