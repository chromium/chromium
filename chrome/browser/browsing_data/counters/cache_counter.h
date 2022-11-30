// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_CACHE_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_CACHE_COUNTER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

class CacheCounter : public browsing_data::BrowsingDataCounter {
 public:
  class CacheResult : public FinishedResult {
   public:
    CacheResult(const CacheCounter* source,
                int64_t cache_size,
                bool is_upper_limit);

    CacheResult(const CacheResult&) = delete;
    CacheResult& operator=(const CacheResult&) = delete;

    ~CacheResult() override;

    int64_t cache_size() const { return cache_size_; }
    bool is_upper_limit() const { return is_upper_limit_; }

   private:
    int64_t cache_size_;
    bool is_upper_limit_;
  };

  explicit CacheCounter(Profile* profile);
  ~CacheCounter() override;

  const char* GetPrefName() const override;

 private:
  void Count() override;
  void OnCacheSizeCalculated(bool is_upper_limit, int64_t cache_bytes);

  raw_ptr<Profile> profile_;
  int64_t calculated_size_;
  bool is_upper_limit_;
  int pending_sources_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_CACHE_COUNTER_H_
