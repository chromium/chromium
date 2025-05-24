// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_CPU_HISTOGRAM_LOGGER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_CPU_HISTOGRAM_LOGGER_H_

#include <base/functional/callback.h>

#include <memory>

namespace content {

class BrowserChildProcessHost;

}  // namespace content

namespace passage_embeddings {

// Measures the CPU usage of the service process at fixed intervals.
class CpuHistogramLogger {
 public:
  CpuHistogramLogger();
  ~CpuHistogramLogger();

  CpuHistogramLogger(const CpuHistogramLogger&) = delete;
  CpuHistogramLogger& operator=(const CpuHistogramLogger&) = delete;

  // Start logging the histogram usage of the child process hosted in
  // `utility_process_host`.
  void StartLogging(content::BrowserChildProcessHost* utility_process_host,
                    base::RepeatingCallback<bool()> poll_embedder_running);

  // Stop logging histogram usage after the next update. Note that if the child
  // process exits, 0 CPU usage will be recorded for the rest of the interval.
  void StopLoggingAfterNextUpdate();

 private:
  class CpuObserver;
  std::unique_ptr<CpuObserver> cpu_observer_;
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_CPU_HISTOGRAM_LOGGER_H_
