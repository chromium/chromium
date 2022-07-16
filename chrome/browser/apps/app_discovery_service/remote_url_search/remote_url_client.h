// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_CLIENT_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_CLIENT_H_

#include "base/callback.h"
#include "base/values.h"
#include "url/gurl.h"

namespace apps {

// A client that queries the URL provided at construction for app
// recommendations. The intended usage here is to periodically call Fetch, which
// will pass results or an error to its callback.
class RemoteUrlClient {
 public:
  // Possible outcomes of a call to the URL. These values persist to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  enum class Status {
    kOk = 0,
    kMaxValue = kOk,
  };

  using ResultsCallback = base::OnceCallback<void(Status, base::Value)>;

  explicit RemoteUrlClient(const GURL& url);
  ~RemoteUrlClient() = default;

  RemoteUrlClient(const RemoteUrlClient&) = delete;
  RemoteUrlClient& operator=(const RemoteUrlClient&) = delete;

  // Fetch results from the given URL. |callback| will be called in one of two
  // ways:
  // - with Status::kOk and a valid base::Value* if the fetch was successful
  // - with any other Status and a nullptr base::Value* otherwise
  void Fetch(ResultsCallback callback);

 private:
  GURL url_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_CLIENT_H_
