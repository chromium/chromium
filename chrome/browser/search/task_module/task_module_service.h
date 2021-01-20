// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_SERVICE_H_
#define CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_SERVICE_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/search/task_module/task_module.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class PrefRegistrySimple;
class Profile;
namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Downloads tasks for current user from GWS.
class TaskModuleService : public KeyedService {
 public:
  TaskModuleService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const std::string& application_locale);
  TaskModuleService(const TaskModuleService&) = delete;
  ~TaskModuleService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // KeyedService:
  void Shutdown() override;

  using TaskModuleCallback =
      base::OnceCallback<void(task_module::mojom::TaskPtr task)>;
  // Downloads and parses tasks and calls |callback| when done.
  // On success |callback| is called with a populated |TaskModuleData| object
  // of the first task which has not been dismissed. On failure, it is called
  // with nullptr.
  void GetPrimaryTask(task_module::mojom::TaskModuleType task_module_type,
                      TaskModuleCallback callback);
  // Dismisses the task with the given name and remembers that setting.
  void DismissTask(task_module::mojom::TaskModuleType task_module_type,
                   const std::string& task_name);
  // Restores the task with the given name and remembers that setting.
  void RestoreTask(task_module::mojom::TaskModuleType task_module_type,
                   const std::string& task_name);

 private:
  void OnDataLoaded(task_module::mojom::TaskModuleType task_module_type,
                    network::SimpleURLLoader* loader,
                    TaskModuleCallback callback,
                    std::unique_ptr<std::string> response);
  void OnJsonParsed(task_module::mojom::TaskModuleType task_module_type,
                    TaskModuleCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);

  // Returns whether a task with the given name has been dismissed.
  bool IsTaskDismissed(task_module::mojom::TaskModuleType task_module_type,
                       const std::string& task_name);

  Profile* profile_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::string application_locale_;

  base::WeakPtrFactory<TaskModuleService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_SERVICE_H_
