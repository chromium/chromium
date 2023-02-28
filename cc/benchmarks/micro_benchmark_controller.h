// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_MICRO_BENCHMARK_CONTROLLER_H_
#define CC_BENCHMARKS_MICRO_BENCHMARK_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "cc/benchmarks/micro_benchmark.h"

namespace base {
class SingleThreadTaskRunner;
class Value;
}  // namespace base

namespace cc {

class LayerTreeHost;

class CC_EXPORT MicroBenchmarkController {
 public:
  explicit MicroBenchmarkController(LayerTreeHost* host);
  MicroBenchmarkController(const MicroBenchmarkController&) = delete;
  ~MicroBenchmarkController();

  MicroBenchmarkController& operator=(const MicroBenchmarkController&) = delete;

  void DidUpdateLayers();

  // Returns the id of the benchmark on success, 0 otherwise.
  int ScheduleRun(const std::string& micro_benchmark_name,
                  base::Value::Dict settings,
                  MicroBenchmark::DoneCallback callback);

  // Returns true if the message was successfully delivered and handled.
  bool SendMessage(int id, base::Value::Dict message);

  std::vector<std::unique_ptr<MicroBenchmarkImpl>> CreateImplBenchmarks() const;

 private:
  void CleanUpFinishedBenchmarks();
  int GetNextIdAndIncrement();

  raw_ptr<LayerTreeHost> host_;
  std::vector<std::unique_ptr<MicroBenchmark>> benchmarks_;
  static int next_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_controller_task_runner_;
};

}  // namespace cc

#endif  // CC_BENCHMARKS_MICRO_BENCHMARK_CONTROLLER_H_
