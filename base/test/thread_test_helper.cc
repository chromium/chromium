// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/thread_test_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"

namespace base {

ThreadTestHelper::ThreadTestHelper(
    scoped_refptr<SequencedTaskRunner> target_sequence)
    : test_result_(false),
      target_sequence_(std::move(target_sequence)),
      done_event_(WaitableEvent::ResetPolicy::AUTOMATIC,
                  WaitableEvent::InitialState::NOT_SIGNALED) {}

bool ThreadTestHelper::Run() {
  if (!target_sequence_->PostTask(
          FROM_HERE, base::BindOnce(&ThreadTestHelper::RunOnSequence, this))) {
    return false;
  }
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
  done_event_.Wait();
  return test_result_;
}

void ThreadTestHelper::RunTest() { set_test_result(true); }

ThreadTestHelper::~ThreadTestHelper() = default;

void ThreadTestHelper::RunOnSequence() {
  RunTest();
  done_event_.Signal();
}

}  // namespace base
