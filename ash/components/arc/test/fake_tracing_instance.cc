// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_tracing_instance.h"

namespace arc {

FakeTracingInstance::FakeTracingInstance() = default;

FakeTracingInstance::~FakeTracingInstance() = default;

void FakeTracingInstance::QueryAvailableCategories(
    QueryAvailableCategoriesCallback callback) {
  std::move(callback).Run(
      {"gfx",   "input",    "view",   "webview", "wm",  "am",       "sm",
       "audio", "video",    "camera", "hal",     "app", "res",      "dalvik",
       "rs",    "bionic",   "power",  "pm",      "ss",  "database", "network",
       "adb",   "vibrator", "aidl",   "nnapi",   "rro"});
}

void FakeTracingInstance::StartTracing(
    const std::vector<std::string>& categories,
    mojo::ScopedHandle socket,
    StartTracingCallback callback) {
  ++start_count_;
  start_categories_ = categories;
  socket_ = std::move(socket);
  std::move(callback).Run(true /* success */);
}

void FakeTracingInstance::StopTracing(StopTracingCallback callback) {
  ++stop_count_;
  std::move(callback).Run(true /* success */);
}

}  // namespace arc
