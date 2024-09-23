// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TASK_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_TASK_MANAGER_ASH_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi task manager interface. Lives in ash-chrome on the
// UI thread.
class TaskManagerAsh : public mojom::TaskManager {
 public:
  class Observer {
   public:
    // Called after a task manager provider is disconnected.
    virtual void OnTaskManagerProviderDisconnected() = 0;
  };

  TaskManagerAsh();
  TaskManagerAsh(const TaskManagerAsh&) = delete;
  TaskManagerAsh& operator=(const TaskManagerAsh&) = delete;
  ~TaskManagerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::TaskManager> pending_receiver);

  // crosapi::mojom::TaskManager:
  void RegisterTaskManagerProvider(
      mojo::PendingRemote<mojom::TaskManagerProvider> provider,
      const base::UnguessableToken& token) override;
  void ShowTaskManager() override;

  // Sets task refreshing flags. Forward the call to the registered remote
  // providers.
  void SetRefreshFlags(int64_t refresh_flags);

  using GetTaskManagerTasksCallback =
      base::OnceCallback<void(std::vector<crosapi::mojom::TaskPtr>,
                              std::vector<crosapi::mojom::TaskGroupPtr>,
                              const std::optional<std::string>&)>;
  // Gets lacros task data. Forward the call to the registered remote providers.
  void GetTaskManagerTasks(GetTaskManagerTasksCallback callback);

  // Notifies lacros that task manager has been closed in ash. Forward the call
  // to the registered remote providers.
  void OnTaskManagerClosed();

  // Activates the lacros task specified by |task_uuid|. Forward the call to the
  // registered remote providers.
  void ActivateTask(const std::string& task_uuid);

  void RemoveObserver();
  void SetObserver(Observer* observer);

  // Returns true if there is at least one registered task manager providers.
  bool HasRegisteredProviders() const;

 private:
  // Called when a TaskManagerProvider is disconnected.
  void TaskManagerProviderDisconnected(const base::UnguessableToken& token);

  // Called when TaskManagerProvider's version is ready.
  void OnProviderVersionReady(
      const base::UnguessableToken& token,
      std::unique_ptr<mojo::Remote<mojom::TaskManagerProvider>> provider,
      uint32_t interface_version);

  // This class supports any number of connections. This allows TaskManager to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::TaskManager> receivers_;

  // Maintains a list of registered remote task manager providers.
  // This model allows multiple providers to be connected remotely. However,
  // currently, there is only one lacros instance, i.e., one provider to be
  // connected. The task manager code only supports one provider for now.
  std::map<base::UnguessableToken, mojo::Remote<mojom::TaskManagerProvider>>
      task_manager_providers_;

  raw_ptr<Observer> observer_ = nullptr;

  int64_t refresh_flags_ = task_manager::REFRESH_TYPE_NONE;

  // Version of the registered remote task manager providers.
  // Note: We assume all registered remote task manager providers are in
  // the same version.
  uint32_t provider_version_ = 0U;

  base::WeakPtrFactory<TaskManagerAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TASK_MANAGER_ASH_H_
