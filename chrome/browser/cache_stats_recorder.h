// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CACHE_STATS_RECORDER_H_
#define CHROME_BROWSER_CACHE_STATS_RECORDER_H_

#include "chrome/common/cache_stats_recorder.mojom.h"

class CacheStatsRecorder : public chrome::mojom::CacheStatsRecorder {
 public:
  explicit CacheStatsRecorder(int render_process_id);
  ~CacheStatsRecorder() override;

  static void Create(
      int render_process_id,
      mojo::PendingAssociatedReceiver<chrome::mojom::CacheStatsRecorder>
          receiver);

 private:
  // chrome::mojom::CacheStatsRecorder:
  void RecordCacheStats(uint64_t capacity, uint64_t size) override;

  const int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(CacheStatsRecorder);
};

#endif  // CHROME_BROWSER_CACHE_STATS_RECORDER_H_
