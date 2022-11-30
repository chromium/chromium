// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_task_graph_runner.h"

namespace cc {

TestTaskGraphRunner::TestTaskGraphRunner() {
  Start("TestTaskGraphRunner", base::SimpleThread::Options());
}

TestTaskGraphRunner::~TestTaskGraphRunner() {
  Shutdown();
}

}  // namespace cc
