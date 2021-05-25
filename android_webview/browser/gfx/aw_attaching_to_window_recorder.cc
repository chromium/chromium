// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_attaching_to_window_recorder.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"

namespace android_webview {

namespace {

constexpr base::TimeDelta kPingInterval[] = {
    base::TimeDelta::FromSeconds(5),
    base::TimeDelta::FromSeconds(30) - base::TimeDelta::FromSeconds(5),
    base::TimeDelta::FromMinutes(3) - base::TimeDelta::FromSeconds(30)};

constexpr size_t kInterval5s = 0;
constexpr size_t kInterval30s = 1;
constexpr size_t kInterval3m = 2;

}  // namespace

AwAttachingToWindowRecorder::AwAttachingToWindowRecorder()
    : created_time_(base::TimeTicks::Now()),
      thread_pool_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DETACH_FROM_SEQUENCE(thread_pool_sequence_checker_);
}

AwAttachingToWindowRecorder::~AwAttachingToWindowRecorder() = default;

void AwAttachingToWindowRecorder::Start() {
  // Can't post the task in constructor, because at least one reference of this
  // object is needed.
  bool result = thread_pool_runner_->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&AwAttachingToWindowRecorder::Ping, this, kInterval5s),
      kPingInterval[kInterval5s]);
  DCHECK(result);
}

void AwAttachingToWindowRecorder::OnAttachedToWindow() {
  thread_pool_runner_->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&AwAttachingToWindowRecorder::RecordAttachedToWindow, this,
                     base::TimeTicks::Now() - created_time_));
}

void AwAttachingToWindowRecorder::OnDestroyed() {
  thread_pool_runner_->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&AwAttachingToWindowRecorder::RecordEverAttachedToWindow,
                     this));
}

void AwAttachingToWindowRecorder::RecordAttachedToWindow(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_sequence_checker_);
  if (was_attached_)
    return;
  was_attached_ = true;
  UMA_HISTOGRAM_MEDIUM_TIMES("Android.WebView.AttachedToWindowTime", time);
}

void AwAttachingToWindowRecorder::RecordEverAttachedToWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_sequence_checker_);
  UMA_HISTOGRAM_BOOLEAN("Android.WebView.EverAttachedToWindow", was_attached_);
}

void AwAttachingToWindowRecorder::Ping(size_t interval_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_sequence_checker_);
  switch (interval_index) {
    case kInterval5s:
      UMA_HISTOGRAM_BOOLEAN("Android.WebView.AttachedToWindowIn5s",
                            was_attached_);
      break;
    case kInterval30s:
      UMA_HISTOGRAM_BOOLEAN("Android.WebView.AttachedToWindowIn30s",
                            was_attached_);
      break;
    case kInterval3m:
      UMA_HISTOGRAM_BOOLEAN("Android.WebView.AttachedToWindowIn3m",
                            was_attached_);
      break;
  }

  if (++interval_index == base::size(kPingInterval))
    return;

  bool result = thread_pool_runner_->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&AwAttachingToWindowRecorder::Ping, this, interval_index),
      kPingInterval[interval_index]);
  DCHECK(result);
}

}  // namespace android_webview
