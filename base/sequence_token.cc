// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_token.h"

#include "base/atomic_sequence_num.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {

namespace {

base::AtomicSequenceNumber g_sequence_token_generator;

base::AtomicSequenceNumber g_task_token_generator;

ABSL_CONST_INIT thread_local SequenceToken current_sequence_token;
ABSL_CONST_INIT thread_local TaskToken current_task_token;

}  // namespace

bool SequenceToken::operator==(const SequenceToken& other) const {
  return token_ == other.token_ && IsValid();
}

bool SequenceToken::operator!=(const SequenceToken& other) const {
  return !(*this == other);
}

bool SequenceToken::IsValid() const {
  return token_ != kInvalidSequenceToken;
}

int SequenceToken::ToInternalValue() const {
  return token_;
}

SequenceToken SequenceToken::Create() {
  return SequenceToken(g_sequence_token_generator.GetNext());
}

SequenceToken SequenceToken::GetForCurrentThread() {
  return current_sequence_token;
}

bool TaskToken::operator==(const TaskToken& other) const {
  return token_ == other.token_ && IsValid();
}

bool TaskToken::operator!=(const TaskToken& other) const {
  return !(*this == other);
}

bool TaskToken::IsValid() const {
  return token_ != kInvalidTaskToken;
}

TaskToken TaskToken::Create() {
  return TaskToken(g_task_token_generator.GetNext());
}

TaskToken TaskToken::GetForCurrentThread() {
  return current_task_token;
}

ScopedSetSequenceTokenForCurrentThread::ScopedSetSequenceTokenForCurrentThread(
    const SequenceToken& sequence_token)
    // The lambdas here exist because invalid tokens don't compare equal, so
    // passing invalid sequence/task tokens as the third args to AutoReset
    // constructors doesn't work.
    : sequence_token_resetter_(&current_sequence_token,
                               [&sequence_token]() {
                                 DCHECK(!current_sequence_token.IsValid());
                                 return sequence_token;
                               }()),
      task_token_resetter_(&current_task_token, [] {
        DCHECK(!current_task_token.IsValid());
        return TaskToken::Create();
      }()) {}

ScopedSetSequenceTokenForCurrentThread::
    ~ScopedSetSequenceTokenForCurrentThread() = default;

}  // namespace base
