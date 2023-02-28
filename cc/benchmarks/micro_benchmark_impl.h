// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_MICRO_BENCHMARK_IMPL_H_
#define CC_BENCHMARKS_MICRO_BENCHMARK_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "cc/cc_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {

class LayerTreeHostImpl;
class LayerImpl;
class PictureLayerImpl;
class CC_EXPORT MicroBenchmarkImpl {
 public:
  using DoneCallback = base::OnceCallback<void(base::Value::Dict)>;

  explicit MicroBenchmarkImpl(
      DoneCallback callback,
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner);
  virtual ~MicroBenchmarkImpl();

  bool IsDone() const;
  virtual void DidCompleteCommit(LayerTreeHostImpl* host);

  virtual void RunOnLayer(LayerImpl* layer);
  virtual void RunOnLayer(PictureLayerImpl* layer);

 protected:
  void NotifyDone(base::Value::Dict result);

 private:
  DoneCallback callback_;
  bool is_done_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
};

}  // namespace cc

#endif  // CC_BENCHMARKS_MICRO_BENCHMARK_IMPL_H_
