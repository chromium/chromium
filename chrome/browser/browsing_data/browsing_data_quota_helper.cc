// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"

#include "base/location.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo() {}

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo(const std::string& host)
    : host(host) {}

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo(const std::string& host,
                                              int64_t temporary_usage,
                                              int64_t persistent_usage,
                                              int64_t syncable_usage)
    : host(host),
      temporary_usage(temporary_usage),
      persistent_usage(persistent_usage),
      syncable_usage(syncable_usage) {}

BrowsingDataQuotaHelper::QuotaInfo::~QuotaInfo() {}

// static
void BrowsingDataQuotaHelperDeleter::Destruct(
    const BrowsingDataQuotaHelper* helper) {
  base::DeleteSoon(FROM_HERE, {BrowserThread::IO}, helper);
}

BrowsingDataQuotaHelper::BrowsingDataQuotaHelper() {}

BrowsingDataQuotaHelper::~BrowsingDataQuotaHelper() {
}

bool BrowsingDataQuotaHelper::QuotaInfo::operator <(
    const BrowsingDataQuotaHelper::QuotaInfo& rhs) const {
  if (this->host != rhs.host)
    return this->host < rhs.host;
  if (this->temporary_usage != rhs.temporary_usage)
    return this->temporary_usage < rhs.temporary_usage;
  if (this->syncable_usage != rhs.syncable_usage)
      return this->syncable_usage < rhs.syncable_usage;
  return this->persistent_usage < rhs.persistent_usage;
}

bool BrowsingDataQuotaHelper::QuotaInfo::operator ==(
    const BrowsingDataQuotaHelper::QuotaInfo& rhs) const {
  return this->host == rhs.host &&
      this->temporary_usage == rhs.temporary_usage &&
      this->persistent_usage == rhs.persistent_usage &&
      this->syncable_usage == rhs.syncable_usage;
}
