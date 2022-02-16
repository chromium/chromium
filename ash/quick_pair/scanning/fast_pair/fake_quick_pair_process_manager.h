// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAKE_QUICK_PAIR_PROCESS_MANAGER_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAKE_QUICK_PAIR_PROCESS_MANAGER_H_

#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/callback_helpers.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeQuickPairProcessManager
    : public ash::quick_pair::QuickPairProcessManager {
 public:
  explicit FakeQuickPairProcessManager(
      base::test::SingleThreadTaskEnvironment* task_environment);

  ~FakeQuickPairProcessManager() override;

  std::unique_ptr<ProcessReference> GetProcessReference(
      ProcessStoppedCallback on_process_stopped_callback) override;

  void SetProcessStopped(bool process_stopped);

 private:
  bool process_stopped_ = false;
  mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>
      data_parser_remote_;
  mojo::PendingRemote<ash::quick_pair::mojom::FastPairDataParser>
      fast_pair_data_parser_;
  std::unique_ptr<ash::quick_pair::FastPairDataParser> data_parser_;
  base::test::SingleThreadTaskEnvironment* task_enviornment_;
  ProcessStoppedCallback on_process_stopped_callback_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAKE_QUICK_PAIR_PROCESS_MANAGER_H_
