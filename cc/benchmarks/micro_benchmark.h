// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_MICRO_BENCHMARK_H_
#define CC_BENCHMARKS_MICRO_BENCHMARK_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "cc/cc_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {
class LayerTreeHost;
class PictureLayer;
class MicroBenchmarkImpl;

class CC_EXPORT MicroBenchmark {
 public:
  using DoneCallback = base::OnceCallback<void(base::Value::Dict)>;

  explicit MicroBenchmark(DoneCallback callback);
  virtual ~MicroBenchmark();

  bool IsDone() const;
  virtual void DidUpdateLayers(LayerTreeHost* layer_tree_host);
  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  virtual void RunOnLayer(PictureLayer* layer);

  virtual bool ProcessMessage(base::Value::Dict message);

  bool ProcessedForBenchmarkImpl() const;
  std::unique_ptr<MicroBenchmarkImpl> GetBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner);

 protected:
  void NotifyDone(base::Value::Dict result);

  virtual std::unique_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner);

 private:
  DoneCallback callback_;
  bool is_done_ = false;
  bool processed_for_benchmark_impl_ = false;
  int id_ = 0;
};

}  // namespace cc

#endif  // CC_BENCHMARKS_MICRO_BENCHMARK_H_
