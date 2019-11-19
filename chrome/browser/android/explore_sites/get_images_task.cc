// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_images_task.h"

#include <tuple>

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

const char kSelectCategoryImagesSql[] = R"(SELECT favicon
FROM sites
LEFT JOIN site_blacklist ON (sites.url = site_blacklist.url)
WHERE category_id = ? AND LENGTH(favicon) > 0 AND NOT removed
AND site_blacklist.url IS NULL
LIMIT ?;)";

const char kSelectSummaryImagesSql[] = R"(SELECT favicon
FROM sites
INNER JOIN categories ON (sites.category_id = categories.category_id)
LEFT JOIN site_blacklist ON (sites.url = site_blacklist.url)
WHERE LENGTH(favicon) > 0 AND NOT removed
AND version_token = ?
AND site_blacklist.url IS NULL
ORDER BY sites.category_id ASC, sites.site_id ASC
LIMIT ?;)";

const char kSelectSiteImageSql[] =
    "SELECT favicon FROM sites WHERE site_id = ?;";

}  // namespace

EncodedImageList GetCategoryImagesSync(int category_id,
                                       int max_images,
                                       sql::Database* db) {
  DCHECK(db);

  sql::Statement category_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectCategoryImagesSql));
  category_statement.BindInt64(0, category_id);
  category_statement.BindInt64(1, max_images);

  EncodedImageList result;
  while (category_statement.Step()) {
    int byte_length = category_statement.ColumnByteLength(0);
    result.push_back(std::make_unique<std::vector<uint8_t>>(byte_length));
    category_statement.ColumnBlobAsVector(0, result.back().get());
  }
  if (!category_statement.Succeeded())
    return EncodedImageList();
  return result;
}

EncodedImageList GetSummaryImagesSync(int max_images, sql::Database* db) {
  DCHECK(db);

  std::string current_version_token, downloading_version_token;
  std::tie(current_version_token, downloading_version_token) =
      ExploreSitesSchema::GetVersionTokens(db);

  std::string token_to_use = current_version_token.empty()
                                 ? downloading_version_token
                                 : current_version_token;

  if (token_to_use.empty())
    return EncodedImageList();

  sql::Statement category_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectSummaryImagesSql));
  category_statement.BindString(0, token_to_use);
  category_statement.BindInt64(1, max_images);

  EncodedImageList result;
  while (category_statement.Step()) {
    int byte_length = category_statement.ColumnByteLength(0);
    result.push_back(std::make_unique<std::vector<uint8_t>>(byte_length));
    category_statement.ColumnBlobAsVector(0, result.back().get());
  }
  if (!category_statement.Succeeded())
    return EncodedImageList();
  return result;
}

EncodedImageList GetSiteImageSync(int site_id, sql::Database* db) {
  DCHECK(db);

  sql::Statement site_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectSiteImageSql));
  site_statement.BindInt64(0, site_id);

  EncodedImageList result;
  while (site_statement.Step()) {
    int byte_length = site_statement.ColumnByteLength(0);
    result.push_back(std::make_unique<std::vector<uint8_t>>(byte_length));
    site_statement.ColumnBlobAsVector(0, result.back().get());
  }
  if (!site_statement.Succeeded())
    return EncodedImageList();
  return result;
}

GetImagesTask::GetImagesTask(ExploreSitesStore* store,
                             int site_id,
                             EncodedImageListCallback callback)
    : store_(store),
      data_type_(DataType::kSite),
      id_(site_id),
      max_results_(1),
      callback_(std::move(callback)) {}

GetImagesTask::GetImagesTask(ExploreSitesStore* store,
                             int category_id,
                             int max_images,
                             EncodedImageListCallback callback)
    : store_(store),
      data_type_(DataType::kCategory),
      id_(category_id),
      max_results_(max_images),
      callback_(std::move(callback)) {}

GetImagesTask::GetImagesTask(ExploreSitesStore* store,
                             DataType data_type,
                             int max_images,
                             EncodedImageListCallback callback)
    : store_(store),
      data_type_(data_type),
      id_(0),
      max_results_(max_images),
      callback_(std::move(callback)) {}

GetImagesTask::~GetImagesTask() = default;

void GetImagesTask::Run() {
  switch (data_type_) {
    case DataType::kCategory:
      store_->Execute(base::BindOnce(&GetCategoryImagesSync, id_, max_results_),
                      base::BindOnce(&GetImagesTask::FinishedExecuting,
                                     weak_ptr_factory_.GetWeakPtr()),
                      EncodedImageList());
      break;
    case DataType::kSite:
      store_->Execute(base::BindOnce(&GetSiteImageSync, id_),
                      base::BindOnce(&GetImagesTask::FinishedExecuting,
                                     weak_ptr_factory_.GetWeakPtr()),
                      EncodedImageList());
      break;
    case DataType::kSummary:
      store_->Execute(base::BindOnce(&GetSummaryImagesSync, max_results_),
                      base::BindOnce(&GetImagesTask::FinishedExecuting,
                                     weak_ptr_factory_.GetWeakPtr()),
                      EncodedImageList());
  }
}

void GetImagesTask::FinishedExecuting(EncodedImageList images) {
  TaskComplete();
  DVLOG(1) << "Finished getting images, resulting in " << images.size()
           << " images.";
  std::move(callback_).Run(std::move(images));
}

}  // namespace explore_sites
