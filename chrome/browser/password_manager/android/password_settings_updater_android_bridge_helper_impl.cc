// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper_impl.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace password_manager {

namespace {

using SyncingAccount =
    PasswordSettingsUpdaterAndroidBridgeHelper::SyncingAccount;

}

std::unique_ptr<PasswordSettingsUpdaterAndroidBridgeHelper>
PasswordSettingsUpdaterAndroidBridgeHelper::Create() {
  return std::make_unique<PasswordSettingsUpdaterAndroidBridgeHelperImpl>();
}

PasswordSettingsUpdaterAndroidBridgeHelperImpl::
    PasswordSettingsUpdaterAndroidBridgeHelperImpl()
    : receiver_bridge_(PasswordSettingsUpdaterAndroidReceiverBridge::Create()),
      dispatcher_bridge_(
          PasswordSettingsUpdaterAndroidDispatcherBridge::Create()),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Bridge is manually shut down on the sequence where all operations are
  // executed. It's safe to use `base::Unretained(dispatcher_bridge_)` for
  // binding.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordSettingsUpdaterAndroidDispatcherBridge::Init,
                     base::Unretained(dispatcher_bridge_.get()),
                     receiver_bridge_->GetJavaBridge()));
}

PasswordSettingsUpdaterAndroidBridgeHelperImpl::
    PasswordSettingsUpdaterAndroidBridgeHelperImpl(
        base::PassKey<class PasswordSettingsUpdaterAndroidBridgeHelperImplTest>,
        std::unique_ptr<PasswordSettingsUpdaterAndroidReceiverBridge>
            receiver_bridge,
        std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
            dispatcher_bridge)
    : receiver_bridge_(std::move(receiver_bridge)),
      dispatcher_bridge_(std::move(dispatcher_bridge)),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordSettingsUpdaterAndroidDispatcherBridge::Init,
                     base::Unretained(dispatcher_bridge_.get()),
                     receiver_bridge_->GetJavaBridge()));
}

PasswordSettingsUpdaterAndroidBridgeHelperImpl::
    ~PasswordSettingsUpdaterAndroidBridgeHelperImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Delete dispatcher bridge on the background thread where it lives.
  bool will_delete = background_task_runner_->DeleteSoon(
      FROM_HERE, std::move(dispatcher_bridge_));

  if (!will_delete) {
    NOTREACHED_IN_MIGRATION()
        << "Failed to post bridge deletion on background thread.";
    base::debug::DumpWithoutCrashing(FROM_HERE);
  }
}

void PasswordSettingsUpdaterAndroidBridgeHelperImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(receiver_bridge_);
  receiver_bridge_->SetConsumer(consumer);
}

void PasswordSettingsUpdaterAndroidBridgeHelperImpl::GetPasswordSettingValue(
    std::optional<SyncingAccount> account,
    PasswordManagerSetting setting) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordSettingsUpdaterAndroidDispatcherBridge::
                         GetPasswordSettingValue,
                     base::Unretained(dispatcher_bridge_.get()),
                     std::move(account), setting));
}

void PasswordSettingsUpdaterAndroidBridgeHelperImpl::SetPasswordSettingValue(
    std::optional<SyncingAccount> account,
    PasswordManagerSetting setting,
    bool value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordSettingsUpdaterAndroidDispatcherBridge::
                         SetPasswordSettingValue,
                     base::Unretained(dispatcher_bridge_.get()),
                     std::move(account), setting, value));
}

}  // namespace password_manager
