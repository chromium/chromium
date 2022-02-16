// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fake_quick_pair_process_manager.h"

namespace ash {
namespace quick_pair {

FakeQuickPairProcessManager::FakeQuickPairProcessManager(
    base::test::SingleThreadTaskEnvironment* task_environment)
    : task_enviornment_(task_environment) {
  data_parser_ = std::make_unique<ash::quick_pair::FastPairDataParser>(
      fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

  data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                           task_enviornment_->GetMainThreadTaskRunner());
}

FakeQuickPairProcessManager::~FakeQuickPairProcessManager() = default;

std::unique_ptr<FakeQuickPairProcessManager::ProcessReference>
FakeQuickPairProcessManager::GetProcessReference(
    FakeQuickPairProcessManager::ProcessStoppedCallback
        on_process_stopped_callback) {
  on_process_stopped_callback_ = std::move(on_process_stopped_callback);

  if (process_stopped_) {
    std::move(on_process_stopped_callback_)
        .Run(ash::quick_pair::QuickPairProcessManager::ShutdownReason::kCrash);
  }

  return std::make_unique<
      ash::quick_pair::QuickPairProcessManagerImpl::ProcessReferenceImpl>(
      data_parser_remote_, base::DoNothing());
}

void FakeQuickPairProcessManager::SetProcessStopped(bool process_stopped) {
  process_stopped_ = process_stopped;
}

}  // namespace quick_pair
}  // namespace ash
