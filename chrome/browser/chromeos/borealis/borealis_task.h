// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_TASK_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_TASK_H_

namespace borealis {

class BorealisContext;

// BorealisTasks are collections of operations that are run by the
// Borealis Context Manager.
class BorealisTask {
 public:
  // Callback to be run when the task completes. The |bool| should reflect
  // if the task succeeded (true) or failed (false).
  using CompletionStatusCallback = base::OnceCallback<void(bool)>;
  virtual void Run(BorealisContext* context,
                   CompletionStatusCallback callback) = 0;
  virtual ~BorealisTask() = default;
};
}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_TASK_H_
