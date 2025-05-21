// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_backend.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

AndroidSmsOtpBackend::AndroidSmsOtpBackend()
    : receiver_bridge_(AndroidSmsOtpFetchReceiverBridge::Create()),
      dispatcher_bridge_(AndroidSmsOtpFetchDispatcherBridge::Create()),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  InitBridges();
}

AndroidSmsOtpBackend::AndroidSmsOtpBackend(
    base::PassKey<class AndroidSmsOtpBackendTest>,
    std::unique_ptr<AndroidSmsOtpFetchReceiverBridge> receiver_bridge,
    std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge> dispatcher_bridge,
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : receiver_bridge_(std::move(receiver_bridge)),
      dispatcher_bridge_(std::move(dispatcher_bridge)),
      background_task_runner_(background_task_runner) {
  InitBridges();
}

AndroidSmsOtpBackend::~AndroidSmsOtpBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Delete dispatcher bridge on the background thread where it lives.
  background_task_runner_->DeleteSoon(FROM_HERE, std::move(dispatcher_bridge_));
}

void AndroidSmsOtpBackend::RetrieveSmsOtp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!initialization_result_.has_value()) {
    // The downstream backend initialization is in progress, postpone the call.
    pending_fetch_request_ = true;
    return;
  }

  // Return early if the downstream backend did not initialize successfully.
  if (!initialization_result_.value()) {
    return;
  }

  // The dispatcher bridge is deleted manually in this class' destructor on the
  // sequence where all operations of this class are executed. It's safe to use
  // `base::Unretained(dispatcher_bridge_)` for binding here.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AndroidSmsOtpFetchDispatcherBridge::RetrieveSmsOtp,
                     base::Unretained(dispatcher_bridge_.get())));
}

void AndroidSmsOtpBackend::OnOtpValueRetrieved(std::string value) {
  // TODO(crbug.com/415271020): Implement.
}

void AndroidSmsOtpBackend::OnOtpValueRetrievalError(
    SmsOtpRetrievalApiErrorCode error_code) {
  // TODO(crbug.com/415271020): Implement.
}

void AndroidSmsOtpBackend::InitBridges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  receiver_bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  // The dispatcher bridge is deleted manually in this class' destructor on the
  // sequence where all operations of this class are executed. It's safe to use
  // `base::Unretained(dispatcher_bridge_)` for binding here.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AndroidSmsOtpFetchDispatcherBridge::Init,
                     base::Unretained(dispatcher_bridge_.get()),
                     receiver_bridge_->GetJavaBridge()),
      base::BindOnce(&AndroidSmsOtpBackend::OnBridgesInitComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidSmsOtpBackend::OnBridgesInitComplete(bool init_success) {
  initialization_result_ = init_success;

  if (init_success && pending_fetch_request_) {
    pending_fetch_request_ = false;
    RetrieveSmsOtp();
  }
}
