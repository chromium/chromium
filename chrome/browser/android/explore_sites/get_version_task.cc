// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_version_task.h"

#include <string>
#include <tuple>

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {

std::string GetVersionSync(sql::Database* db) {
  if (!db)
    return "";

  std::string catalog_version_token, downloading_version_token;
  std::tie(catalog_version_token, downloading_version_token) =
      ExploreSitesSchema::GetVersionTokens(db);

  if (!downloading_version_token.empty())
    return downloading_version_token;
  return catalog_version_token;
}

GetVersionTask::GetVersionTask(ExploreSitesStore* store,
                               base::OnceCallback<void(std::string)> callback)
    : store_(store), callback_(std::move(callback)) {}

GetVersionTask::~GetVersionTask() = default;

void GetVersionTask::Run() {
  store_->Execute(base::BindOnce(&GetVersionSync),
                  base::BindOnce(&GetVersionTask::FinishedExecuting,
                                 weak_ptr_factory_.GetWeakPtr()),
                  std::string());
}

void GetVersionTask::FinishedExecuting(std::string catalog_version_token) {
  TaskComplete();
  std::move(callback_).Run(catalog_version_token);
}

}  // namespace explore_sites
