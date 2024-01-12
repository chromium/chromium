// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_token.h"

#include "base/atomic_sequence_num.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base::internal {

namespace {

base::AtomicSequenceNumber g_sequence_token_generator;

base::AtomicSequenceNumber g_task_token_generator;

ABSL_CONST_INIT thread_local SequenceToken current_sequence_token;
ABSL_CONST_INIT thread_local TaskToken current_task_token;
ABSL_CONST_INIT thread_local bool current_task_is_thread_bound = true;

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
  if (!current_sequence_token.IsValid()) {
    current_sequence_token = SequenceToken::Create();
    DCHECK(current_task_is_thread_bound);
  }
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

bool CurrentTaskIsThreadBound() {
  return current_task_is_thread_bound;
}

TaskScope::TaskScope(SequenceToken sequence_token, bool is_single_threaded)
    : previous_task_token_(TaskToken::GetForCurrentThread()),
      previous_sequence_token_(SequenceToken::GetForCurrentThread()),
      previous_task_is_thread_bound_(current_task_is_thread_bound) {
  current_task_token = TaskToken::Create();
  current_sequence_token = sequence_token;
  current_task_is_thread_bound = is_single_threaded;
}

TaskScope::~TaskScope() {
  current_task_token = previous_task_token_;
  current_sequence_token = previous_sequence_token_;
  current_task_is_thread_bound = previous_task_is_thread_bound_;
}

}  // namespace base::internal
