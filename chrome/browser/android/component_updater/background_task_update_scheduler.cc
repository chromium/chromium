// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/component_updater/background_task_update_scheduler.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/android/chrome_jni_headers/UpdateScheduler_jni.h"

namespace component_updater {

namespace {

// Delay of running component updates after the background task fires to give
// enough time for async component registration.
const base::TimeDelta kOnStartTaskDelay = base::TimeDelta::FromSeconds(2);

}  // namespace

// static
bool BackgroundTaskUpdateScheduler::IsAvailable() {
  return Java_UpdateScheduler_isAvailable(base::android::AttachCurrentThread());
}

BackgroundTaskUpdateScheduler::BackgroundTaskUpdateScheduler() {
  DCHECK(IsAvailable());
  JNIEnv* env = base::android::AttachCurrentThread();
  j_update_scheduler_.Reset(Java_UpdateScheduler_getInstance(env));
  Java_UpdateScheduler_setNativeScheduler(env, j_update_scheduler_,
                                          reinterpret_cast<intptr_t>(this));
}

BackgroundTaskUpdateScheduler::~BackgroundTaskUpdateScheduler() = default;

void BackgroundTaskUpdateScheduler::Schedule(
    const base::TimeDelta& initial_delay,
    const base::TimeDelta& delay,
    const UserTask& user_task,
    const OnStopTaskCallback& on_stop) {
  user_task_ = user_task;
  on_stop_ = on_stop;
  Java_UpdateScheduler_schedule(
      base::android::AttachCurrentThread(), j_update_scheduler_,
      initial_delay.InMilliseconds(), delay.InMilliseconds());
}

void BackgroundTaskUpdateScheduler::Stop() {
  Java_UpdateScheduler_cancelTask(base::android::AttachCurrentThread(),
                                  j_update_scheduler_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void BackgroundTaskUpdateScheduler::OnStartTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  // Component registration is async. Add some delay to give some time for the
  // registration.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BackgroundTaskUpdateScheduler::OnStartTaskDelayed,
                     weak_ptr_factory_.GetWeakPtr()),
      kOnStartTaskDelay);
}

void BackgroundTaskUpdateScheduler::OnStopTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(on_stop_);
  on_stop_.Run();
}

void BackgroundTaskUpdateScheduler::OnStartTaskDelayed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!user_task_) {
    LOG(WARNING) << "No components registered to update";
    Java_UpdateScheduler_finishTask(env, j_update_scheduler_,
                                    /*reschedule=*/false);
    return;
  }
  user_task_.Run(base::BindOnce(&Java_UpdateScheduler_finishTask,
                                base::Unretained(env), j_update_scheduler_,
                                /*reschedule=*/true));
}

}  // namespace component_updater
