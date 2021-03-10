// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TASK_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TASK_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_launch_watcher.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace borealis {

class BorealisContext;

// BorealisTasks are collections of operations that are run by the
// Borealis Context Manager.
class BorealisTask {
 public:
  // Callback to be run when the task completes. The |result| should reflect
  // if the task succeeded with kSuccess and an empty string. If it fails, a
  // different result should be used, and an error string provided.
  using CompletionResultCallback =
      base::OnceCallback<void(BorealisStartupResult, std::string)>;
  BorealisTask();
  BorealisTask(const BorealisTask&) = delete;
  BorealisTask& operator=(const BorealisTask&) = delete;
  virtual ~BorealisTask();

  void Run(BorealisContext* context, CompletionResultCallback callback);

 protected:
  virtual void RunInternal(BorealisContext* context) = 0;

  void Complete(BorealisStartupResult result, std::string message);

 private:
  CompletionResultCallback callback_;
};

// Mounts the Borealis DLC.
class MountDlc : public BorealisTask {
 public:
  MountDlc();
  ~MountDlc() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnMountDlc(
      BorealisContext* context,
      const chromeos::DlcserviceClient::InstallResult& install_result);
  base::WeakPtrFactory<MountDlc> weak_factory_{this};
};

// Creates a disk image for the Borealis VM.
class CreateDiskImage : public BorealisTask {
 public:
  CreateDiskImage();
  ~CreateDiskImage() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnCreateDiskImage(
      BorealisContext* context,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> response);
  base::WeakPtrFactory<CreateDiskImage> weak_factory_{this};
};

// Instructs Concierge to start the Borealis VM.
class StartBorealisVm : public BorealisTask {
 public:
  StartBorealisVm();
  ~StartBorealisVm() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnStartBorealisVm(
      BorealisContext* context,
      base::Optional<vm_tools::concierge::StartVmResponse> response);
  base::WeakPtrFactory<StartBorealisVm> weak_factory_{this};
};

// Waits for the startup daemon to signal completion.
class AwaitBorealisStartup : public BorealisTask {
 public:
  AwaitBorealisStartup(Profile* profile, std::string vm_name);
  ~AwaitBorealisStartup() override;
  void RunInternal(BorealisContext* context) override;
  BorealisLaunchWatcher& GetWatcherForTesting();

 private:
  void OnAwaitBorealisStartup(BorealisContext* context,
                              base::Optional<std::string> container);
  BorealisLaunchWatcher watcher_;
  base::WeakPtrFactory<AwaitBorealisStartup> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TASK_H_
