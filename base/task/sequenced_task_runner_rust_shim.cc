// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner_rust_shim.h"

#include "base/functional/bind.h"
#include "base/functional/callback.rs.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

SequencedTaskRunner* GetCurrentDefaultSequencedTaskRunnerForRust() {
  // Make a copy of the refptr to increment its count.
  scoped_refptr<base::SequencedTaskRunner> ptr =
      base::SequencedTaskRunner::GetCurrentDefault();
  // Convert the scoped_refptr to a raw pointer without decrementing its count.
  // This ensures the object won't get deleted before rust has a change to wrap
  // it.
  return ptr.release();
}

bool PostTaskFromRust(SequencedTaskRunner& runner,
                      rust::Box<base::RustOnceClosure> task) {
  return runner.PostTask(
      FROM_HERE, base::BindOnce(&base::RustOnceClosure::run, std::move(task)));
}

void AddRef(const SequencedTaskRunner& runner) {
  runner.AddRef();
}

void Release(const SequencedTaskRunner& runner) {
  runner.Release();
}

}  // namespace base
