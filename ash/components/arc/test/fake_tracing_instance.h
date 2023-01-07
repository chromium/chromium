// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_TRACING_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_TRACING_INSTANCE_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/tracing.mojom.h"
#include "mojo/public/cpp/system/handle.h"

namespace arc {

class FakeTracingInstance : public mojom::TracingInstance {
 public:
  FakeTracingInstance();
  ~FakeTracingInstance() override;

  FakeTracingInstance(const FakeTracingInstance&) = delete;
  FakeTracingInstance& operator=(const FakeTracingInstance&) = delete;

  // mojom::TracingInstance:
  void QueryAvailableCategories(
      QueryAvailableCategoriesCallback callback) override;
  void StartTracing(const std::vector<std::string>& categories,
                    mojo::ScopedHandle socket,
                    StartTracingCallback callback) override;
  void StopTracing(StopTracingCallback callback) override;

  int start_count() const { return start_count_; }
  int stop_count() const { return stop_count_; }
  mojo::Handle socket() const { return socket_.get(); }
  const std::vector<std::string>& start_categories() {
    return start_categories_;
  }

 private:
  int start_count_ = 0;
  std::vector<std::string> start_categories_;
  mojo::ScopedHandle socket_;
  int stop_count_ = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_TRACING_INSTANCE_H_
