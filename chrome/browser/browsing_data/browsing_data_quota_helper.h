// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_H_

#include <stdint.h>

#include <list>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"

class BrowsingDataQuotaHelper;
class Profile;

struct BrowsingDataQuotaHelperDeleter {
  static void Destruct(const BrowsingDataQuotaHelper* helper);
};

// This class is an interface class to bridge between Cookies Tree and Unified
// Quota System.  This class provides a way to get usage and quota information
// through the instance.
//
// Call Create to create an instance for a profile and call StartFetching with
// a callback to fetch information asynchronously.
//
// Parallel fetching is not allowed, a fetching task should start after end of
// previous task.  All method of this class should called from UI thread.
class BrowsingDataQuotaHelper
    : public base::RefCountedThreadSafe<BrowsingDataQuotaHelper,
                                        BrowsingDataQuotaHelperDeleter> {
 public:
  // QuotaInfo contains host-based quota and usage information for persistent
  // and temporary storage.
  struct QuotaInfo {
    QuotaInfo();
    explicit QuotaInfo(const std::string& host);
    QuotaInfo(const std::string& host,
              int64_t temporary_usage,
              int64_t persistent_usage,
              int64_t syncable_usage);
    ~QuotaInfo();

    // Certain versions of MSVC 2008 have bad implementations of ADL for nested
    // classes so they require these operators to be declared here instead of in
    // the global namespace.
    bool operator <(const QuotaInfo& rhs) const;
    bool operator ==(const QuotaInfo& rhs) const;

    std::string host;
    int64_t temporary_usage = 0;
    int64_t persistent_usage = 0;
    int64_t syncable_usage = 0;
  };

  using QuotaInfoArray = std::list<QuotaInfo>;
  using FetchResultCallback = base::OnceCallback<void(const QuotaInfoArray&)>;

  static BrowsingDataQuotaHelper* Create(Profile* profile);

  virtual void StartFetching(FetchResultCallback callback) = 0;

  virtual void RevokeHostQuota(const std::string& host) = 0;

 protected:
  BrowsingDataQuotaHelper();
  virtual ~BrowsingDataQuotaHelper();

 private:
  friend class base::DeleteHelper<BrowsingDataQuotaHelper>;
  friend struct BrowsingDataQuotaHelperDeleter;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataQuotaHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_H_
