// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_database_helper.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "storage/common/database/database_identifier.h"

using content::BrowserContext;
using content::BrowserThread;
using storage::DatabaseIdentifier;

BrowsingDataDatabaseHelper::DatabaseInfo::DatabaseInfo(
    const DatabaseIdentifier& identifier,
    const std::string& database_name,
    const std::string& description,
    int64_t size,
    base::Time last_modified)
    : identifier(identifier),
      database_name(database_name),
      description(description),
      size(size),
      last_modified(last_modified) {}

BrowsingDataDatabaseHelper::DatabaseInfo::DatabaseInfo(
    const DatabaseInfo& other) = default;

BrowsingDataDatabaseHelper::DatabaseInfo::~DatabaseInfo() {}

BrowsingDataDatabaseHelper::BrowsingDataDatabaseHelper(Profile* profile)
    : tracker_(BrowserContext::GetDefaultStoragePartition(profile)
                   ->GetDatabaseTracker()) {}

BrowsingDataDatabaseHelper::~BrowsingDataDatabaseHelper() {
}

void BrowsingDataDatabaseHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      tracker_->task_runner(), FROM_HERE,
      base::BindOnce(
          [](storage::DatabaseTracker* tracker) {
            std::list<DatabaseInfo> result;
            std::vector<storage::OriginInfo> origins_info;
            if (tracker->GetAllOriginsInfo(&origins_info)) {
              for (const storage::OriginInfo& origin : origins_info) {
                DatabaseIdentifier identifier =
                    DatabaseIdentifier::Parse(origin.GetOriginIdentifier());
                // Non-websafe state is not considered browsing data.
                if (!BrowsingDataHelper::HasWebScheme(identifier.ToOrigin()))
                  continue;
                std::vector<base::string16> databases;
                origin.GetAllDatabaseNames(&databases);
                for (const base::string16& db : databases) {
                  base::FilePath file_path = tracker->GetFullDBFilePath(
                      origin.GetOriginIdentifier(), db);
                  base::File::Info file_info;
                  if (base::GetFileInfo(file_path, &file_info)) {
                    result.push_back(DatabaseInfo(
                        identifier, base::UTF16ToUTF8(db),
                        base::UTF16ToUTF8(origin.GetDatabaseDescription(db)),
                        file_info.size, file_info.last_modified));
                  } else {
                    // This is an incognito database, so the file is not
                    // accessible. This browsing data record will not be
                    // user-visible, but is enumerated by test code, so produce
                    // a dummy record for testing.
                    result.push_back(DatabaseInfo(
                        identifier, base::UTF16ToUTF8(db),
                        base::UTF16ToUTF8(origin.GetDatabaseDescription(db)), 0,
                        base::Time()));
                  }
                }
              }
            }
            return result;
          },
          base::RetainedRef(tracker_)),
      std::move(callback));
}

void BrowsingDataDatabaseHelper::DeleteDatabase(const std::string& origin,
                                                const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(
                                    &storage::DatabaseTracker::DeleteDatabase),
                                tracker_, origin, base::UTF8ToUTF16(name),
                                net::CompletionCallback()));
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::PendingDatabaseInfo(
    const GURL& origin,
    const std::string& name,
    const std::string& description)
    : origin(origin),
      name(name),
      description(description) {
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::~PendingDatabaseInfo() {}

bool CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::operator<(
    const PendingDatabaseInfo& other) const {
  return std::tie(origin, name) < std::tie(other.origin, other.name);
}

CannedBrowsingDataDatabaseHelper::CannedBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile) {
}

void CannedBrowsingDataDatabaseHelper::AddDatabase(
    const GURL& origin,
    const std::string& name,
    const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.
  pending_database_info_.insert(PendingDatabaseInfo(origin, name, description));
}

void CannedBrowsingDataDatabaseHelper::Reset() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_database_info_.clear();
}

bool CannedBrowsingDataDatabaseHelper::empty() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_database_info_.empty();
}

size_t CannedBrowsingDataDatabaseHelper::GetDatabaseCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_database_info_.size();
}

const std::set<CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo>&
CannedBrowsingDataDatabaseHelper::GetPendingDatabaseInfo() {
  return pending_database_info_;
}

void CannedBrowsingDataDatabaseHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<DatabaseInfo> result;
  for (const PendingDatabaseInfo& info : pending_database_info_) {
    DatabaseIdentifier identifier =
        DatabaseIdentifier::CreateFromOrigin(info.origin);

    result.push_back(
        DatabaseInfo(identifier, info.name, info.description, 0, base::Time()));
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataDatabaseHelper::DeleteDatabase(
    const std::string& origin_identifier,
    const std::string& name) {
  GURL origin =
      storage::DatabaseIdentifier::Parse(origin_identifier).ToOrigin();
  for (auto it = pending_database_info_.begin();
       it != pending_database_info_.end(); ++it) {
    if (it->origin == origin && it->name == name) {
      pending_database_info_.erase(it);
      break;
    }
  }
  BrowsingDataDatabaseHelper::DeleteDatabase(origin_identifier, name);
}

CannedBrowsingDataDatabaseHelper::~CannedBrowsingDataDatabaseHelper() {}
