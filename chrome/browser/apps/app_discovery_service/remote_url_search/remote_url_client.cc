// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_client.h"

namespace apps {

RemoteUrlClient::RemoteUrlClient(const GURL& url) : url_(url) {}

void RemoteUrlClient::Fetch(ResultsCallback callback) {
  // TODO(crbug.com/1244221): Unimplemented.
  std::move(callback).Run(Status::kOk, base::Value());
}

}  // namespace apps
