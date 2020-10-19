// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_TASK_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_TASK_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace borealis {

class BorealisContext;

// BorealisTasks are collections of operations that are run by the
// Borealis Context Manager.
class BorealisTask {
 public:
  // Callback to be run when the task completes. The |status| should reflect
  // if the task succeeded with kSuccess and an empty string. If it fails, a
  // different status should be used, and an error string provided.
  using CompletionStatusCallback =
      base::OnceCallback<void(BorealisContextManager::Status, std::string)>;
  BorealisTask() = default;
  BorealisTask(const BorealisTask&) = delete;
  BorealisTask& operator=(const BorealisTask&) = delete;
  virtual void Run(BorealisContext* context,
                   CompletionStatusCallback callback) = 0;
  virtual ~BorealisTask() = default;
};

// Mounts the Borealis DLC.
class MountDlc : public BorealisTask {
 public:
  MountDlc();
  ~MountDlc() override;
  void Run(BorealisContext* context,
           CompletionStatusCallback callback) override;

 private:
  void OnMountDlc(
      BorealisContext* context,
      CompletionStatusCallback callback,
      const chromeos::DlcserviceClient::InstallResult& install_result);
  base::WeakPtrFactory<MountDlc> weak_factory_{this};
};

// Creates a disk image for the Borealis VM.
class CreateDiskImage : public BorealisTask {
 public:
  CreateDiskImage();
  ~CreateDiskImage() override;
  void Run(BorealisContext* context,
           CompletionStatusCallback callback) override;

 private:
  void OnCreateDiskImage(
      BorealisContext* context,
      CompletionStatusCallback callback,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> response);
  base::WeakPtrFactory<CreateDiskImage> weak_factory_{this};
};

// Instructs Concierge to start the Borealis VM.
class StartBorealisVm : public BorealisTask {
 public:
  StartBorealisVm();
  ~StartBorealisVm() override;
  void Run(BorealisContext* context,
           CompletionStatusCallback callback) override;

 private:
  void OnStartBorealisVm(
      BorealisContext* context,
      CompletionStatusCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> response);
  base::WeakPtrFactory<StartBorealisVm> weak_factory_{this};
};

// Waits for the startup daemon to signal completion.
class AwaitBorealisStartup : public BorealisTask {
 public:
  void Run(BorealisContext* context,
           CompletionStatusCallback callback) override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_TASK_H_
