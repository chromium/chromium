// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_FAKE_BRUSCHETTA_LAUNCHER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_FAKE_BRUSCHETTA_LAUNCHER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"

namespace bruschetta {

class FakeBruschettaLauncher : public BruschettaLauncher {
 public:
  FakeBruschettaLauncher();
  ~FakeBruschettaLauncher() override;

  void EnsureRunning(
      base::OnceCallback<void(BruschettaResult)> callback) override;

  void set_ensure_running_result(BruschettaResult result) { result_ = result; }

 private:
  BruschettaResult result_ = BruschettaResult::kSuccess;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_FAKE_BRUSCHETTA_LAUNCHER_H_
