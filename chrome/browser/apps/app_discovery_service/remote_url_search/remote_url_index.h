// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_INDEX_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_INDEX_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_client.h"

namespace base {
class Value;
}  // namespace base

namespace apps {
class RemoteUrlClient;

// An index of app recommendations. Uses the RemoteUrlClient given at
// construction to periodically request app recommendations. The results are
// then indexed and made available for querying via the GetApps method.
class RemoteUrlIndex {
 public:
  RemoteUrlIndex(std::unique_ptr<RemoteUrlClient> client,
                 const base::FilePath& storage_path);
  ~RemoteUrlIndex();

  RemoteUrlIndex(const RemoteUrlIndex&) = delete;
  RemoteUrlIndex& operator=(const RemoteUrlIndex&) = delete;

  base::Value* GetApps(const std::string& query);

 private:
  void MaybeUpdateAndReschedule();
  void OnUpdateComplete(RemoteUrlClient::Status status, base::Value value);

  const std::unique_ptr<RemoteUrlClient> client_;

  const base::FilePath storage_path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<RemoteUrlIndex> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_INDEX_H_
